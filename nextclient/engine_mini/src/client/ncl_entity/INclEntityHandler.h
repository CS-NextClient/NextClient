#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <tier0/basetypes.h>

#include <ankerl/unordered_dense.h>
#include <goldsrc/fwd.h>

// The vendored ncl_entity headers name edict_t unqualified; this resolves it on the client as type_resolver.h does on the server.
namespace ncl_entity
{
    using edict_t = goldsrc::edict_t;
}

#include <nextclientapi_amxx/ncl_entity.h>

#include "client/ncl_entity/ncl_entity_message.h"

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
