#pragma once
#include <cstdint>
#include <variant>
#include <vector>
#include <utility>
#include <tuple>
#include "ArrayStringIdPairsDelta.h"

namespace ncl_entity
{
    struct FieldValue
    {
        std::variant<
            uint8_t,
            uint16_t,
            uint32_t,
            float,
            std::vector<std::pair<uint16_t, uint16_t>>,
            ArrayStringIdPairsDelta
        > value;

        FieldValue() = default;
        FieldValue(uint8_t v) : value(v) {}
        FieldValue(uint16_t v) : value(v) {}
        FieldValue(uint32_t v) : value(v) {}
        FieldValue(float v) : value(v) {}
        FieldValue(std::vector<std::pair<uint16_t, uint16_t>> v) : value(std::move(v)) {}
        FieldValue(ArrayStringIdPairsDelta v) : value(std::move(v)) {}

        bool operator==(const FieldValue& other) const
        {
            if (value.index() != other.value.index())
            {
                return false;
            }

            switch (value.index())
            {
                case 0:
                    return std::get<uint8_t>(value) == std::get<uint8_t>(other.value);

                case 1:
                    return std::get<uint16_t>(value) == std::get<uint16_t>(other.value);

                case 2:
                    return std::get<uint32_t>(value) == std::get<uint32_t>(other.value);

                case 3:
                    return std::get<float>(value) == std::get<float>(other.value);

                case 4:
                    return std::get<std::vector<std::pair<uint16_t, uint16_t>>>(value) ==
                           std::get<std::vector<std::pair<uint16_t, uint16_t>>>(other.value);

                case 5:
                    return std::get<ArrayStringIdPairsDelta>(value) == std::get<ArrayStringIdPairsDelta>(other.value);
            }

            return false;
        }

        bool operator!=(const FieldValue& other) const
        {
            return !(*this == other);
        }
    };
} // namespace ncl_entity
