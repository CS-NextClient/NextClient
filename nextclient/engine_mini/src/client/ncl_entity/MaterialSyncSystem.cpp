#include "engine.h"
#include "MaterialSyncSystem.h"

#include <algorithm>
#include <format>

#include "cl_ncl_entity_sync.h"
#include "client/cl_string_registry.h"
#include "graphics/render/ServerMaterials.h"

namespace
{
    // The order of this list is the order the fields arrive in when an entity is created,
    // so it has to match the server's declaration exactly.
    constexpr uint16_t FIELD_ENTITY_INDEX = 1u << 0;
    constexpr uint16_t FIELD_MATERIALS = 1u << 1;
    constexpr uint16_t FIELD_PARAMS = 1u << 2;

    std::vector<ClientNclEntityFieldDescriptor> g_Fields = {
        {FIELD_ENTITY_INDEX, ncl_entity::FieldType::UINT},
        {FIELD_MATERIALS, ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA},
        {FIELD_PARAMS, ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA},
    };

    const ncl_entity::ArrayStringIdPairsDelta* DeltaOf(const NclEntityFields& fields, uint16_t field)
    {
        auto it = fields.find(field);

        return it != fields.end() ? &std::get<ncl_entity::ArrayStringIdPairsDelta>(it->second.value) : nullptr;
    }

    void ApplyPairs(const ncl_entity::ArrayStringIdPairsDelta& delta,
                    std::vector<std::pair<uint16_t, uint16_t>>& pairs)
    {
        if (delta.clear)
        {
            pairs.clear();
        }

        for (const auto& pair : delta.removed)
        {
            for (size_t i = pairs.size(); i > 0; i--)
            {
                if (pairs[i - 1] == pair)
                {
                    pairs.erase(pairs.begin() + (i - 1));
                }
            }
        }

        for (const auto& pair : delta.added)
        {
            // Idempotent, and capped at what the renderer keeps. Dress rebinds the whole
            // accumulated list on every update, and every rebind reads a preset, finds a
            // material by name and deep-copies it: a pair added twice is that work done
            // twice for the life of the entity, and a list past the ceiling is that work
            // done for a binding the renderer will refuse.
            if (std::find(pairs.begin(), pairs.end(), pair) != pairs.end())
            {
                continue;
            }

            if (pairs.size() >= render::kMaxBindingsPerEntity)
            {
                break;
            }

            pairs.push_back(pair);
        }
    }

    void ApplyParams(const ncl_entity::ArrayStringIdPairsDelta& delta,
                     ankerl::unordered_dense::map<uint16_t, uint16_t>& params)
    {
        if (delta.clear)
        {
            params.clear();
        }

        for (const auto& pair : delta.removed)
        {
            params.erase(pair.first);
        }

        for (const auto& pair : delta.added)
        {
            params[pair.first] = pair.second;
        }
    }
}

const std::vector<ClientNclEntityFieldDescriptor>& MaterialSyncSystem::Fields() const
{
    return g_Fields;
}

void MaterialSyncSystem::Redress(uint32_t entity_index) const
{
    if (entity_index == 0)
    {
        return;
    }

    int index = static_cast<int>(entity_index);

    render::ServerMaterials_ClearBindings(index);

    for (const auto& [entity_id, dressed] : entities_)
    {
        if (dressed.entity_index != entity_index)
        {
            continue;
        }

        for (const auto& [preset_id, name_id] : dressed.materials)
        {
            const char* preset = CL_StringRegistryGetString(preset_id);
            const char* name = CL_StringRegistryGetString(name_id);

            if (preset != nullptr && name != nullptr)
            {
                render::ServerMaterials_SetBinding(index, preset, name);
            }
        }

        for (const auto& [key_id, steps] : dressed.params)
        {
            const char* key = CL_StringRegistryGetString(key_id);

            if (key != nullptr)
            {
                // Signed again: the server wrote a step count, which can be either side of
                // the parameter's zero, through an unsigned field.
                render::ServerMaterials_SetParam(index, key, static_cast<int16_t>(steps));
            }
        }
    }
}

void MaterialSyncSystem::OnCreate(uint16_t entity_id, const NclEntityFields& fields)
{
    auto index_it = fields.find(FIELD_ENTITY_INDEX);

    if (index_it == fields.end())
    {
        return;
    }

    Dressed dressed;
    dressed.entity_index = std::get<uint32_t>(index_it->second.value);

    if (const ncl_entity::ArrayStringIdPairsDelta* materials = DeltaOf(fields, FIELD_MATERIALS))
    {
        ApplyPairs(*materials, dressed.materials);
    }

    if (const ncl_entity::ArrayStringIdPairsDelta* params = DeltaOf(fields, FIELD_PARAMS))
    {
        ApplyParams(*params, dressed.params);
    }

    // Recorded before it is put on: Redress builds the index out of the entities it knows.
    uint32_t entity_index = dressed.entity_index;

    entities_[entity_id] = std::move(dressed);

    Redress(entity_index);
}

void MaterialSyncSystem::OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields)
{
    auto it = entities_.find(entity_id);

    if (it == entities_.end())
    {
        return;
    }

    Dressed& dressed = it->second;
    uint32_t previous_index = dressed.entity_index;

    if (field_mask & FIELD_ENTITY_INDEX)
    {
        auto index_it = fields.find(FIELD_ENTITY_INDEX);

        if (index_it != fields.end())
        {
            dressed.entity_index = std::get<uint32_t>(index_it->second.value);
        }
    }

    if (field_mask & FIELD_MATERIALS)
    {
        if (const ncl_entity::ArrayStringIdPairsDelta* materials = DeltaOf(fields, FIELD_MATERIALS))
        {
            ApplyPairs(*materials, dressed.materials);
        }
    }

    if (field_mask & FIELD_PARAMS)
    {
        if (const ncl_entity::ArrayStringIdPairsDelta* params = DeltaOf(fields, FIELD_PARAMS))
        {
            ApplyParams(*params, dressed.params);
        }
    }

    // The index it left has to be rebuilt too: whoever else reports it is still wearing
    // something, and this entity's share of it has just gone.
    if (previous_index != dressed.entity_index)
    {
        Redress(previous_index);
    }

    Redress(dressed.entity_index);
}

void MaterialSyncSystem::OnDestroy(uint16_t entity_id)
{
    auto it = entities_.find(entity_id);

    if (it == entities_.end())
    {
        return;
    }

    uint32_t entity_index = it->second.entity_index;

    entities_.erase(it);

    Redress(entity_index);
}

void MaterialSyncSystem::OnClear()
{
    for (const auto& [entity_id, dressed] : entities_)
    {
        if (dressed.entity_index != 0)
        {
            render::ServerMaterials_ClearBindings(static_cast<int>(dressed.entity_index));
        }
    }

    entities_.clear();
}

void MaterialSyncSystem::CollectDebugInfo(std::vector<std::string>& out_lines)
{
    out_lines.push_back(std::format("MaterialSync: {} dressed entities", entities_.size()));

    for (const auto& [entity_id, dressed] : entities_)
    {
        out_lines.push_back(std::format(
            "  sync {} -> entity {}: {} materials, {} params",
            entity_id, dressed.entity_index, dressed.materials.size(), dressed.params.size()));
    }
}
