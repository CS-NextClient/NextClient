#pragma once
#include <cstdint>

namespace tex
{
    struct PaletteHandle
    {
        uint32_t index = UINT32_MAX;
        uint32_t generation = 1;

        bool IsValid() const
        {
            return index != UINT32_MAX;
        }

        bool operator==(const PaletteHandle& other) const
        {
            return index == other.index && generation == other.generation;
        }

        bool operator!=(const PaletteHandle& other) const
        {
            return !(*this == other);
        }

        static constexpr PaletteHandle Invalid()
        {
            return {UINT32_MAX, 1};
        }
    };
} // namespace tex
