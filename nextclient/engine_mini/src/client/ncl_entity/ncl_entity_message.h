#pragma once

#include <cstdint>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <data_types/MessageReader.h>
#include <nextclientapi_amxx/ncl_entity/FieldType.h>
#include <nextclientapi_amxx/ncl_entity/FieldValue.h>

struct ClientNclEntityFieldDescriptor
{
    uint16_t field_bit{};
    ncl_entity::FieldType type = ncl_entity::FieldType::BYTE;
};

using NclEntityFields = ankerl::unordered_dense::map<uint16_t, ncl_entity::FieldValue>;

namespace NclEntityMessage
{
    bool ReadFields(
        MessageReader& reader,
        const std::vector<ClientNclEntityFieldDescriptor>& descriptors,
        bool update,
        uint16_t field_mask,
        NclEntityFields& fields
    );
}
