#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <array>
#include <span>

#include <easylogging++.h>
#include <ankerl/unordered_dense.h>
#include <EASTL/fixed_string.h>

#include "Texture.h"
#include "TextureHandle.h"
#include "TextureLifetime.h"

#include "common/sys_dll.h"
#include "graphics/texture_loader.h"
#include "graphics/gl_local.h"
#include "resource/palette_manager/PaletteManager.h"

namespace tex
{
    // Slot lifecycle (session):
    //   Free  --Load-->  Active  --ClearSession-->  Cached  --Evict-->  Free
    //                    Active  --Destroy-->  Free
    //
    // Persistent slots are always Active once loaded and never evicted.
    //
    // Cached slots keep the GL texture alive on the GPU so that BSP/model structures
    // can continue referencing old texnums. Eviction destroys the GL texture.
    template <uint32_t PersistentCapacity, uint32_t SessionCapacity>
    class TextureManager
    {
        enum class SlotState : uint8_t
        {
            Free,
            Active,
            Cached
        };

        struct TextureSlot
        {
            uint32_t generation = 1;
            TextureLifetime lifetime = TextureLifetime::Session;
            SlotState state = SlotState::Free;

            // LRU doubly-linked list (session slots only)
            int32_t lru_prev = -1;
            int32_t lru_next = -1;

            // Free list singly-linked
            int32_t free_next = -1;

            Texture texture{};
        };

        struct TexIdentifierHash
        {
            using is_avalanching = void;
            using is_transparent = void;

            uint64_t operator()(std::string_view s) const noexcept
            {
                return ankerl::unordered_dense::hash<std::string_view>{}(s);
            }

            uint64_t operator()(TexIdentifierStr const& s) const noexcept
            {
                return (*this)(std::string_view(s.data(), s.size()));
            }
        };

        static constexpr uint32_t TotalCapacity = PersistentCapacity + SessionCapacity;

        std::array<TextureSlot, TotalCapacity> slots_{};

        ankerl::unordered_dense::map<int, uint32_t> gl_id_to_index_;
        ankerl::unordered_dense::map<TexIdentifierStr, uint32_t, TexIdentifierHash> identifier_to_index_;

        int32_t persistent_free_head_ = -1;
        int32_t session_free_head_ = -1;

        int32_t lru_head_ = -1;
        int32_t lru_tail_ = -1;

        uint32_t session_used_count_ = 0; // Active + Cached

        std::span<TextureSlot, PersistentCapacity> persistent_slots_;
        std::span<TextureSlot, SessionCapacity> session_slots_;

    public:
        explicit TextureManager();

        /// Loads or reuses a texture by identifier. If a texture with the same identifier
        /// already exists (Active or Cached), returns the existing handle (reactivating Cached
        /// slots). Otherwise allocates a new slot, uploads via tex::LoadTexture, and returns
        /// the handle. Returns TextureHandle::Invalid() on upload failure.
        TextureHandle Load(
            const TexIdentifierStr& identifier,
            TextureLifetime lifetime,
            TextureFormat format,
            int width,
            int height,
            const uint8_t* data,
            bool mipmap,
            const uint8_t* palette,
            int filter
        );

        /// Returns the Texture for handle, or nullptr if the handle is stale or the slot is not Active.
        Texture* Get(TextureHandle handle);

        /// Returns the Texture by its GL texture name (texnum), or nullptr if not found / not Active.
        Texture* GetByGLId(int texnum);

        /// Iterates over all non-Free slots, invoking callable(Texture*, TextureHandle, TextureLifetime).
        /// If callable returns bool, iteration stops when it returns false.
        template <class TCallable>
        void ForEach(TCallable&& callable);

        /// Destroys a single texture: deletes the GL object, releases the palette,
        /// removes index mappings, and returns the slot to the free list.
        void Destroy(TextureHandle handle);

        /// Transitions all Active session slots to Cached without deleting GL textures.
        /// Cached slots retain their GL objects until evicted by AllocateSlot when pool
        /// space is needed. This allows Load() to reactivate a Cached texture by
        /// identifier without re-uploading it.
        void ClearSession();

    private:
        uint32_t AllocateSlot(TextureLifetime lifetime);
        void FreeSlot(uint32_t index);
        void EvictOne();

        void LruPushBack(uint32_t index);
        void LruRemove(uint32_t index);
    };
} // namespace tex

#include "TextureManager.tpp"

namespace tex
{
    inline TextureManager<1024, 10000> g_TextureManagerGlob;
}
