#pragma once
#include "engine.h"

#include <string>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "INclEntityHandler.h"

// What a server has dressed an entity in, as state of that entity.
//
// A type of its own rather than a field of the weapon: a material is not a property of
// being a weapon, and the renderer that applies it knows only entity indices. Anything the
// server can bind an edict to can wear one.
class MaterialSyncSystem : public INclEntityHandler
{
    struct Dressed
    {
        // The index the renderer knows the drawn model by. Tracked by the server from the
        // bound edict, so it follows the entity rather than being announced.
        uint32_t entity_index = 0;

        // Pairs of string ids: which preset a material came from and its name there.
        std::vector<std::pair<uint16_t, uint16_t>> materials;

        // Instance values, keyed by the string id of "uWear" or "texture/uWear".
        ankerl::unordered_dense::map<uint16_t, uint16_t> params;
    };

    ankerl::unordered_dense::map<uint16_t, Dressed> entities_;

public:
    const std::vector<ClientNclEntityFieldDescriptor>& Fields() const override;
    void OnCreate(uint16_t entity_id, const NclEntityFields& fields) override;
    void OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields) override;
    void OnDestroy(uint16_t entity_id) override;
    void OnClear() override;
    void CollectDebugInfo(std::vector<std::string>& out_lines) override;

private:
    // Puts everything this entity wears on the renderer, in one order: the materials first,
    // then what is written over them. The renderer remakes its copy of a material whenever
    // the material is set, so the two always go together and in this order.
    void Dress(const Dressed& dressed) const;
};
