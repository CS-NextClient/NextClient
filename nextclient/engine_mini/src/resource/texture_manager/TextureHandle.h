#pragma once
#include <cstdint>

namespace tex
{
    struct TextureHandle
    {
        uint32_t index = UINT32_MAX;
        uint32_t generation = 1;

        bool IsValid() const
        {
            return index != UINT32_MAX;
        }

        static constexpr TextureHandle Invalid()
        {
            return {UINT32_MAX, 1};
        }
    };
} // namespace tex
