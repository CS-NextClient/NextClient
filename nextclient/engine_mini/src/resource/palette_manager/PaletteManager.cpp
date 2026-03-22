#include "PaletteManager.h"

#include "common/sys_dll.h"

#include <optick.h>

namespace tex
{
    PaletteManager::PaletteManager()
    {
        for (uint32_t i = 0; i < kCapacity; ++i)
        {
            slots[i].free_next = (int32_t)(i + 1);
        }

        slots[kCapacity - 1].free_next = -1;
        free_head = 0;
    }

    uint64_t PaletteManager::HashPalette(const uint8_t* colors)
    {
        OPTICK_EVENT();

        uint64_t hash = 0xcbf29ce484222325ULL;

        for (int i = 0; i < (int)sizeof(Palette::colors); ++i)
        {
            hash ^= colors[i];
            hash *= 0x100000001b3ULL;
        }

        return hash;
    }

    PaletteHandle PaletteManager::Load(const uint8_t* colors)
    {
        OPTICK_EVENT();

        if (!colors)
        {
            return PaletteHandle::Invalid();
        }

        uint64_t hash = HashPalette(colors);

        auto it = hash_to_index.find(hash);
        if (it != hash_to_index.end())
        {
            PaletteSlot& slot = slots[it->second];

            if (slot.loaded && std::memcmp(slot.palette.colors, colors, sizeof(Palette::colors)) == 0)
            {
                slot.refcount++;
                return {it->second, slot.generation};
            }
        }

        uint32_t index = AllocateSlot();
        PaletteSlot& slot = slots[index];

        std::memcpy(slot.palette.colors, colors, sizeof(Palette::colors));
        slot.loaded = true;
        slot.refcount = 1;

        hash_to_index[hash] = index;

        return {index, slot.generation};
    }

    Palette* PaletteManager::Get(PaletteHandle handle)
    {
        OPTICK_EVENT();

        if (!handle.IsValid() || handle.index >= kCapacity)
        {
            return nullptr;
        }

        PaletteSlot& slot = slots[handle.index];

        if (!slot.loaded || slot.generation != handle.generation)
        {
            return nullptr;
        }

        return &slot.palette;
    }

    void PaletteManager::Release(PaletteHandle handle)
    {
        OPTICK_EVENT();

        if (!handle.IsValid() || handle.index >= kCapacity)
        {
            return;
        }

        PaletteSlot& slot = slots[handle.index];

        if (!slot.loaded || slot.generation != handle.generation)
        {
            return;
        }

        if (slot.refcount > 0)
        {
            slot.refcount--;
        }

        if (slot.refcount == 0)
        {
            FreeSlot(handle.index);
        }
    }

    uint32_t PaletteManager::AllocateSlot()
    {
        OPTICK_EVENT();

        if (free_head == -1)
        {
            Sys_Error("[PaletteManager] Palette pool exhausted (%u slots)", kCapacity);
        }

        uint32_t index = free_head;
        free_head = slots[index].free_next;
        slots[index].free_next = -1;

        return index;
    }

    void PaletteManager::FreeSlot(uint32_t index)
    {
        OPTICK_EVENT();

        PaletteSlot& slot = slots[index];

        uint64_t hash = HashPalette(slot.palette.colors);
        hash_to_index.erase(hash);

        slot.loaded = false;
        slot.generation++;
        slot.refcount = 0;
        slot.free_next = free_head;
        free_head = index;
    }
} // namespace tex
