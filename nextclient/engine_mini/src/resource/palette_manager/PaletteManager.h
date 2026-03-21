#pragma once
#include <cstdint>
#include <cstring>
#include <array>
#include <unordered_map>

#include "Palette.h"
#include "PaletteHandle.h"

namespace tex
{
    class PaletteManager
    {
        static constexpr uint32_t Capacity = 256;

        struct PaletteSlot
        {
            uint32_t generation = 1;
            uint32_t refcount = 0;
            int32_t free_next = -1;
            bool loaded = false;
            Palette palette;
        };

        std::array<PaletteSlot, Capacity> slots{};
        std::unordered_map<uint64_t, uint32_t> hash_to_index;
        int32_t free_head = -1;

    public:
        PaletteManager();

        /// Loads a 768-byte RGB palette (256 entries). If an identical palette
        /// already exists (matched by FNV-1a hash + memcmp), increments its refcount
        /// and returns the existing handle. Otherwise allocates a new slot.
        /// Returns PaletteHandle::Invalid() if colors is nullptr.
        PaletteHandle Load(const uint8_t* colors);

        /// Returns the Palette for handle, or nullptr if the handle is stale or the slot is not loaded.
        Palette* Get(PaletteHandle handle);

        /// Decrements the refcount for handle. When refcount reaches zero,
        /// the slot is freed and the hash mapping is removed.
        void Release(PaletteHandle handle);

    private:
        static uint64_t HashPalette(const uint8_t* colors);
        uint32_t AllocateSlot();
        void FreeSlot(uint32_t index);
    };

    inline PaletteManager g_PaletteManagerGlob;
} // namespace tex
