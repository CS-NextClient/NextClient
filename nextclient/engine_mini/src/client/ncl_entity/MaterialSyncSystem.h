#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "client/ncl_entity/INclEntityHandler.h"

class ClientStringRegistry;

namespace render
{
    class ServerMaterials;
}

class MaterialSyncSystem : public INclEntityHandler
{
    struct Dressed
    {
        // The index the renderer knows the drawn model by. Tracked by the server from the
        // bound edict, so it follows the entity rather than being announced.
        uint32_t entity_index{};

        // Full synchronized set of preset/material ids. The renderer limits resolved textures.
        std::vector<std::pair<uint16_t, uint16_t>> materials;

        // Instance values, keyed by the string id of "uWear" or "texture/uWear".
        ankerl::unordered_dense::map<uint16_t, float> params;
    };

    render::ServerMaterials& materials_;
    const ClientStringRegistry& strings_;
    ankerl::unordered_dense::map<uint16_t, Dressed> entities_;

public:
    MaterialSyncSystem(render::ServerMaterials& materials, const ClientStringRegistry& strings);
    const std::vector<ClientNclEntityFieldDescriptor>& Fields() const override;
    void OnCreate(uint16_t entity_id, const NclEntityFields& fields) override;
    void OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields) override;
    void OnDestroy(uint16_t entity_id) override;
    void OnClear() override;
    void CollectDebugInfo(std::vector<std::string>& out_lines) override;

private:
    void RemoveOwner(uint16_t entity_id, uint32_t entity_index);
    bool ReplaceOwner(uint16_t entity_id, const Dressed& dressed) const;
    bool ReplaceOwnerParams(uint16_t entity_id, const Dressed& dressed) const;
};
