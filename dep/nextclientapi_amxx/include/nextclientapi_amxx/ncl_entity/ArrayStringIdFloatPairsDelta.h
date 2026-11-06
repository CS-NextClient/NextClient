#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace ncl_entity
{
    struct ArrayStringIdFloatPairsDelta
    {
        std::vector<std::pair<uint16_t, float>> added{};
        std::vector<uint16_t> removed{};
        bool clear{};

        bool operator==(const ArrayStringIdFloatPairsDelta& other) const
        {
            return clear == other.clear && added == other.added && removed == other.removed;
        }

        bool operator!=(const ArrayStringIdFloatPairsDelta& other) const
        {
            return !(*this == other);
        }
    };
} // namespace ncl_entity
