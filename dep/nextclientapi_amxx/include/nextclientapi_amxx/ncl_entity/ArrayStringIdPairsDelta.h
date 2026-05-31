#pragma once
#include <cstdint>
#include <vector>

namespace ncl_entity
{
    struct ArrayStringIdPairsDelta
    {
        std::vector<std::pair<uint16_t, uint16_t>> added{};
        std::vector<std::pair<uint16_t, uint16_t>> removed{};
        bool clear{};

        bool operator==(const ArrayStringIdPairsDelta& other) const
        {
            return clear == other.clear && added == other.added && removed == other.removed;
        }

        bool operator!=(const ArrayStringIdPairsDelta& other) const
        {
            return !(*this == other);
        }
    };
} // namespace ncl_entity
