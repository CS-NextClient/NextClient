#include "client/ncl_entity/ncl_entity_message.h"

#include <cmath>

namespace
{
    bool ReadValue(MessageReader& reader, ncl_entity::FieldType type, bool update, ncl_entity::FieldValue& value)
    {
        using namespace ncl_entity;
        switch (type)
        {
            case FieldType::BYTE:
                value = FieldValue{static_cast<uint8_t>(reader.ReadByte())};
                break;
            case FieldType::USHORT:
            case FieldType::STRING_ID:
                value = FieldValue{static_cast<uint16_t>(reader.ReadShort())};
                break;
            case FieldType::UINT:
                value = FieldValue{static_cast<uint32_t>(reader.ReadLong())};
                break;
            case FieldType::FLOAT:
                value = FieldValue{reader.ReadFloat()};
                break;
            case FieldType::COORD:
                value = FieldValue{reader.ReadCoord()};
                break;
            case FieldType::ARRAY_STRING_ID_PAIRS:
            case FieldType::ARRAY_STRING_ID_PAIRS_DELTA:
                {
                    int added = reader.ReadByte();
                    if (added < 0 || (added == 255 && (!update || type != FieldType::ARRAY_STRING_ID_PAIRS_DELTA)))
                    {
                        return false;
                    }
                    ArrayStringIdPairsDelta delta;
                    if (added == 255)
                    {
                        delta.clear = true;
                    }
                    else
                    {
                        for (int i = 0; i < added; ++i)
                        {
                            uint16_t key = static_cast<uint16_t>(reader.ReadShort());
                            uint16_t replacement = static_cast<uint16_t>(reader.ReadShort());
                            delta.added.emplace_back(key, replacement);
                        }
                        int removed = reader.ReadByte();
                        if (!update && removed != 0)
                        {
                            return false;
                        }
                        for (int i = 0; i < removed; ++i)
                        {
                            uint16_t key = static_cast<uint16_t>(reader.ReadShort());
                            uint16_t replacement = static_cast<uint16_t>(reader.ReadShort());
                            delta.removed.emplace_back(key, replacement);
                        }
                        if (type == FieldType::ARRAY_STRING_ID_PAIRS && removed != 0)
                        {
                            return false;
                        }
                    }
                    value = FieldValue{std::move(delta)};
                    break;
                }
            case FieldType::ARRAY_STRING_ID_FLOAT_PAIRS_DELTA:
                {
                    int added = reader.ReadByte();
                    if (added < 0 || (added == 255 && !update))
                    {
                        return false;
                    }
                    ArrayStringIdFloatPairsDelta delta;
                    if (added == 255)
                    {
                        delta.clear = true;
                    }
                    else
                    {
                        for (int i = 0; i < added; ++i)
                        {
                            uint16_t key = static_cast<uint16_t>(reader.ReadShort());
                            float number = reader.ReadFloat();
                            if (!std::isfinite(number))
                            {
                                return false;
                            }
                            delta.added.emplace_back(key, number);
                        }
                        int removed = reader.ReadByte();
                        if (!update && removed != 0)
                        {
                            return false;
                        }
                        for (int i = 0; i < removed; ++i)
                        {
                            delta.removed.push_back(static_cast<uint16_t>(reader.ReadShort()));
                        }
                    }
                    value = FieldValue{std::move(delta)};
                    break;
                }
            default:
                return false;
        }
        return !reader.is_overflowed();
    }
} // namespace

bool NclEntityMessage::ReadFields(
    MessageReader& reader,
    const std::vector<ClientNclEntityFieldDescriptor>& descriptors,
    bool update,
    uint16_t field_mask,
    NclEntityFields& fields
)
{
    NclEntityFields parsed;
    parsed.reserve(descriptors.size());
    uint16_t known_bits = 0;
    for (const ClientNclEntityFieldDescriptor& descriptor : descriptors)
    {
        if (descriptor.field_bit == 0 || (descriptor.field_bit & (descriptor.field_bit - 1)) != 0 || (known_bits & descriptor.field_bit))
        {
            return false;
        }
        known_bits |= descriptor.field_bit;
        if (update && !(descriptor.field_bit & field_mask))
        {
            continue;
        }
        ncl_entity::FieldValue value;
        if (!ReadValue(reader, descriptor.type, update, value))
        {
            return false;
        }
        parsed.emplace(descriptor.field_bit, std::move(value));
    }
    if (reader.is_overflowed() || reader.get_remaining_size() != 0 || (update && (field_mask & ~known_bits)))
    {
        return false;
    }
    fields = std::move(parsed);
    return true;
}
