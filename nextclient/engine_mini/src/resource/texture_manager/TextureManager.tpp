#pragma once
namespace tex
{
    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    TextureManager<PersistentCapacity, SessionCapacity>::TextureManager() :
        persistent_slots_(slots_.data(), PersistentCapacity),
        session_slots_(slots_.data() + PersistentCapacity, SessionCapacity)
    {
        auto init_free_list = [](std::span<TextureSlot> span, int32_t offset) -> int32_t {
            if (span.empty())
            {
                return -1;
            }

            for (size_t i = 0; i < span.size(); i++)
            {
                span[i].free_next = (int32_t)(offset + i + 1);
            }

            span.back().free_next = -1;
            return offset;
        };

        persistent_free_head_ = init_free_list(persistent_slots_, 0);
        session_free_head_ = init_free_list(session_slots_, PersistentCapacity);
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    TextureHandle TextureManager<PersistentCapacity, SessionCapacity>::Load(
        const TexIdentifierStr& identifier,
        TextureLifetime lifetime,
        TextureFormat format,
        int width,
        int height,
        const uint8_t* data,
        bool mipmap,
        const uint8_t* palette,
        int filter
    )
    {
        OPTICK_EVENT();

        auto it = identifier_to_index_.find(identifier);
        if (it != identifier_to_index_.end())
        {
            uint32_t index = it->second;
            TextureSlot& slot = slots_[index];

            if (slot.state != SlotState::Free)
            {
                if (slot.state == SlotState::Cached)
                {
                    slot.state = SlotState::Active;
                }

                if (slot.lifetime == TextureLifetime::Session)
                {
                    LruRemove(index);
                    LruPushBack(index);
                }

                return {index, slot.generation};
            }
        }

        uint32_t index = AllocateSlot(lifetime);
        TextureSlot& slot = slots_[index];

        if (!tex::LoadTexture(slot.texture, identifier, format, data, width, height, mipmap, palette, filter))
        {
            FreeSlot(index);

            Con_DPrintf(
                ConLogType::Error,
                "[TextureManager] Failed to load texture: %s | %dx%d | %s | %s",
                identifier.c_str(),
                width,
                height,
                mipmap ? "mipmap" : "no mipmap",
                palette ? "palette" : "no palette"
            );

            return TextureHandle::Invalid();
        }

        // Con_DPrintf(
        //     ConLogType::Info,
        //     "[TextureManager] Loaded texture: %s | %dx%d | %s | %s | %d | slot: %u",
        //     identifier.c_str(),
        //     width,
        //     height,
        //     mipmap ? "mipmap" : "no mipmap",
        //     palette ? "palette" : "no palette",
        //     slot.texture.texnum,
        //     index
        // );

        slot.state = SlotState::Active;
        gl_id_to_index_[slot.texture.texnum] = index;
        identifier_to_index_[slot.texture.identifier] = index;

        if (lifetime == TextureLifetime::Session)
        {
            session_used_count_++;
            LruPushBack(index);
        }

        return {index, slot.generation};
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    Texture* TextureManager<PersistentCapacity, SessionCapacity>::Get(TextureHandle handle)
    {
        OPTICK_EVENT();

        if (handle.index >= slots_.size())
        {
            return nullptr;
        }

        TextureSlot& slot = slots_[handle.index];

        if (slot.generation != handle.generation)
        {
            return nullptr;
        }

        if (slot.state != SlotState::Active)
        {
            return nullptr;
        }

        return &slot.texture;
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    Texture* TextureManager<PersistentCapacity, SessionCapacity>::GetByGLId(int texnum)
    {
        OPTICK_EVENT();

        auto it = gl_id_to_index_.find(texnum);
        if (it == gl_id_to_index_.end())
        {
            return nullptr;
        }

        uint32_t index = it->second;
        TextureSlot& slot = slots_[index];

        if (slot.state != SlotState::Active)
        {
            return nullptr;
        }

        return &slot.texture;
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    template <class TCallable>
    void TextureManager<PersistentCapacity, SessionCapacity>::ForEach(TCallable&& callable)
    {
        using TResult = std::invoke_result_t<TCallable&, Texture*, TextureHandle, TextureLifetime>;

        for (uint32_t i = 0; i < slots_.size(); i++)
        {
            TextureSlot& slot = slots_[i];

            if (slot.state == SlotState::Free)
            {
                continue;
            }

            TextureHandle handle{i, slot.generation};

            if constexpr (std::is_same_v<TResult, bool>)
            {
                bool result = std::invoke(callable, &slot.texture, handle, slot.lifetime);
                if (!result)
                {
                    return;
                }
            }
            else
            {
                std::invoke(callable, &slot.texture, handle, slot.lifetime);
            }
        }
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    void TextureManager<PersistentCapacity, SessionCapacity>::Destroy(TextureHandle handle)
    {
        OPTICK_EVENT();

        if (handle.index >= slots_.size())
        {
            return;
        }

        TextureSlot& slot = slots_[handle.index];

        if (slot.generation != handle.generation)
        {
            return;
        }

        if (slot.state == SlotState::Free)
        {
            return;
        }

        gl_id_to_index_.erase(slot.texture.texnum);
        identifier_to_index_.erase(slot.texture.identifier);
        tex::UnloadTexture(slot.texture);

        if (slot.lifetime == TextureLifetime::Session)
        {
            LruRemove(handle.index);
            session_used_count_--;
        }

        slot.state = SlotState::Free;
        slot.generation++;

        FreeSlot(handle.index);
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    void TextureManager<PersistentCapacity, SessionCapacity>::ClearSession()
    {
        OPTICK_EVENT();

        int32_t node = lru_head_;

        while (node != -1)
        {
            TextureSlot& slot = slots_[node];

            if (slot.state == SlotState::Active)
            {
                slot.state = SlotState::Cached;
                slot.generation++;
            }

            node = slot.lru_next;
        }
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    uint32_t TextureManager<PersistentCapacity, SessionCapacity>::AllocateSlot(TextureLifetime lifetime)
    {
        OPTICK_EVENT();

        int32_t* free_head = (lifetime == TextureLifetime::Persistent) ? &persistent_free_head_ : &session_free_head_;

        if (lifetime == TextureLifetime::Session && *free_head == -1)
        {
            EvictOne();
        }

        if (*free_head == -1)
        {
            Sys_Error("[TextureManager] Texture pool exhausted (lifetime=%d)", (int)lifetime);
        }

        uint32_t index = *free_head;
        *free_head = slots_[index].free_next;

        slots_[index].free_next = -1;
        slots_[index].lifetime = lifetime;

        return index;
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    void TextureManager<PersistentCapacity, SessionCapacity>::FreeSlot(uint32_t index)
    {
        OPTICK_EVENT();

        TextureSlot& slot = slots_[index];

        int32_t* free_head = (slot.lifetime == TextureLifetime::Persistent) ? &persistent_free_head_ : &session_free_head_;

        slot.free_next = *free_head;
        *free_head = index;
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    void TextureManager<PersistentCapacity, SessionCapacity>::EvictOne()
    {
        OPTICK_EVENT();

        int32_t node = lru_head_;

        while (node != -1)
        {
            TextureSlot& slot = slots_[node];
            int32_t next = slot.lru_next;

            if (slot.state == SlotState::Cached)
            {
                LruRemove(node);

                gl_id_to_index_.erase(slot.texture.texnum);
                identifier_to_index_.erase(slot.texture.identifier);
                tex::UnloadTexture(slot.texture);

                slot.state = SlotState::Free;
                slot.generation++;

                FreeSlot(node);
                session_used_count_--;

                return;
            }

            node = next;
        }

        Sys_Error("[TextureManager] Session pool exhausted: all %u slots are active", SessionCapacity);
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    void TextureManager<PersistentCapacity, SessionCapacity>::LruPushBack(uint32_t index)
    {
        OPTICK_EVENT();

        TextureSlot& slot = slots_[index];

        slot.lru_prev = lru_tail_;
        slot.lru_next = -1;

        if (lru_tail_ != -1)
        {
            slots_[lru_tail_].lru_next = index;
        }

        lru_tail_ = index;

        if (lru_head_ == -1)
        {
            lru_head_ = index;
        }
    }

    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    void TextureManager<PersistentCapacity, SessionCapacity>::LruRemove(uint32_t index)
    {
        OPTICK_EVENT();

        TextureSlot& slot = slots_[index];

        if (slot.lru_prev != -1)
        {
            slots_[slot.lru_prev].lru_next = slot.lru_next;
        }

        if (slot.lru_next != -1)
        {
            slots_[slot.lru_next].lru_prev = slot.lru_prev;
        }

        if (lru_head_ == (int32_t)index)
        {
            lru_head_ = slot.lru_next;
        }

        if (lru_tail_ == (int32_t)index)
        {
            lru_tail_ = slot.lru_prev;
        }

        slot.lru_prev = -1;
        slot.lru_next = -1;
    }
} // namespace tex
