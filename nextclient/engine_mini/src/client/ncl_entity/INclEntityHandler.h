#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <tier0/basetypes.h>
#include <const.h>

#include <ankerl/unordered_dense.h>
#include <nextclientapi_amxx/ncl_entity.h>

struct ClientNclEntityFieldDescriptor
{
    uint16_t field_bit = 0;
    ncl_entity::FieldType type = ncl_entity::FieldType::BYTE;
};

using NclEntityFields = ankerl::unordered_dense::map<uint16_t, ncl_entity::FieldValue>;

struct INclEntityHandler
{
    virtual ~INclEntityHandler() = default;

    virtual const std::vector<ClientNclEntityFieldDescriptor>& Fields() const = 0;

    virtual void OnCreate(uint16_t entity_id, const NclEntityFields& fields) = 0;
    virtual void OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields) = 0;
    virtual void OnDestroy(uint16_t entity_id) = 0;
    virtual void OnClear() = 0;
    virtual void CollectDebugInfo(std::vector<std::string>& out_lines) = 0;
};
