#include "client/ncl_entity/MaterialSyncSystem.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <optional>

#include "client/ClientStringRegistry.h"
#include "graphics/render/material/ServerMaterials.h"

namespace
{
    // The order of this list is the order the fields arrive in when an entity is created,
    // so it has to match the server's declaration exactly.
    constexpr uint16_t kFieldEntityIndex = 1u << 0;
    constexpr uint16_t kFieldMaterials = 1u << 1;
    constexpr uint16_t kFieldParams = 1u << 2;

    std::vector<ClientNclEntityFieldDescriptor> g_Fields = {
        {kFieldEntityIndex, ncl_entity::FieldType::UINT},
        {kFieldMaterials, ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA},
        {kFieldParams, ncl_entity::FieldType::ARRAY_STRING_ID_FLOAT_PAIRS_DELTA},
    };

    const ncl_entity::ArrayStringIdPairsDelta* DeltaOf(const NclEntityFields& fields, uint16_t field)
    {
        auto it = fields.find(field);

        return it != fields.end() ? &std::get<ncl_entity::ArrayStringIdPairsDelta>(it->second.value) : nullptr;
    }

    const ncl_entity::ArrayStringIdFloatPairsDelta* ParamsOf(const NclEntityFields& fields)
    {
        auto it = fields.find(kFieldParams);

        return it != fields.end() ? &std::get<ncl_entity::ArrayStringIdFloatPairsDelta>(it->second.value) : nullptr;
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
            if (std::find(pairs.begin(), pairs.end(), pair) != pairs.end())
            {
                continue;
            }

            pairs.push_back(pair);
        }
    }

    bool ApplyParams(const ncl_entity::ArrayStringIdFloatPairsDelta& delta, ankerl::unordered_dense::map<uint16_t, float>& params)
    {
        for (const auto& [key, value] : delta.added)
        {
            if (!std::isfinite(value))
            {
                return false;
            }
        }

        if (delta.clear)
        {
            params.clear();
        }

        for (uint16_t key : delta.removed)
        {
            params.erase(key);
        }

        for (const auto& pair : delta.added)
        {
            params[pair.first] = pair.second;
        }

        return true;
    }

    std::optional<render::ServerMaterialBindingOwner> BindingOwner(uint16_t entity_id, uint32_t entity_index)
    {
        if (entity_index == 0 || entity_index > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }

        return render::ServerMaterialBindingOwner{static_cast<int>(entity_index), entity_id};
    }

    std::vector<render::ServerMaterialParam> SnapshotParams(const ClientStringRegistry& strings, const ankerl::unordered_dense::map<uint16_t, float>& params)
    {
        std::vector<render::ServerMaterialParam> snapshot;
        snapshot.reserve(params.size());

        for (const auto& [key_id, value] : params)
        {
            if (const char* key = strings.Find(key_id))
            {
                snapshot.push_back({key, value});
            }
        }

        return snapshot;
    }

}

MaterialSyncSystem::MaterialSyncSystem(render::ServerMaterials& materials, const ClientStringRegistry& strings) :
    materials_(materials),
    strings_(strings)
{}

void MaterialSyncSystem::RemoveOwner(uint16_t entity_id, uint32_t entity_index)
{
    if (const std::optional<render::ServerMaterialBindingOwner> owner = BindingOwner(entity_id, entity_index))
    {
        materials_.RemoveOwner(*owner);
    }
}

const std::vector<ClientNclEntityFieldDescriptor>& MaterialSyncSystem::Fields() const
{
    return g_Fields;
}

bool MaterialSyncSystem::ReplaceOwner(uint16_t entity_id, const Dressed& dressed) const
{
    const std::optional<render::ServerMaterialBindingOwner> owner = BindingOwner(entity_id, dressed.entity_index);

    if (!owner.has_value())
    {
        return true;
    }

    std::vector<render::ServerMaterialBinding> bindings;
    bindings.reserve(dressed.materials.size());

    for (const auto& [preset_id, name_id] : dressed.materials)
    {
        const char* preset = strings_.Find(preset_id);
        const char* name = strings_.Find(name_id);

        if (preset != nullptr && name != nullptr)
        {
            bindings.push_back({preset, name});
        }
    }

    const std::vector<render::ServerMaterialParam> params = SnapshotParams(strings_, dressed.params);
    return materials_.ReplaceOwner(*owner, bindings, params);
}

bool MaterialSyncSystem::ReplaceOwnerParams(uint16_t entity_id, const Dressed& dressed) const
{
    const std::optional<render::ServerMaterialBindingOwner> owner = BindingOwner(entity_id, dressed.entity_index);

    if (!owner.has_value())
    {
        return true;
    }

    const std::vector<render::ServerMaterialParam> params = SnapshotParams(strings_, dressed.params);
    return materials_.ReplaceOwnerParams(*owner, params);
}

void MaterialSyncSystem::OnCreate(uint16_t entity_id, const NclEntityFields& fields)
{
    auto index_it = fields.find(kFieldEntityIndex);

    if (index_it == fields.end())
    {
        return;
    }

    Dressed dressed;
    dressed.entity_index = std::get<uint32_t>(index_it->second.value);

    if (const ncl_entity::ArrayStringIdPairsDelta* materials = DeltaOf(fields, kFieldMaterials))
    {
        ApplyPairs(*materials, dressed.materials);
    }

    if (const ncl_entity::ArrayStringIdFloatPairsDelta* params = ParamsOf(fields))
    {
        if (!ApplyParams(*params, dressed.params))
        {
            return;
        }
    }

    if (!ReplaceOwner(entity_id, dressed))
    {
        return;
    }

    auto previous = entities_.find(entity_id);

    if (previous != entities_.end() && previous->second.entity_index != dressed.entity_index)
    {
        RemoveOwner(entity_id, previous->second.entity_index);
    }

    entities_[entity_id] = std::move(dressed);
}

void MaterialSyncSystem::OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields)
{
    auto it = entities_.find(entity_id);

    if (it == entities_.end())
    {
        return;
    }

    Dressed dressed = it->second;
    uint32_t previous_index = dressed.entity_index;
    bool materials_changed = false;
    bool params_changed = false;

    if (field_mask & kFieldEntityIndex)
    {
        auto index_it = fields.find(kFieldEntityIndex);

        if (index_it != fields.end())
        {
            dressed.entity_index = std::get<uint32_t>(index_it->second.value);
        }
    }

    if (field_mask & kFieldMaterials)
    {
        if (const ncl_entity::ArrayStringIdPairsDelta* materials = DeltaOf(fields, kFieldMaterials))
        {
            ApplyPairs(*materials, dressed.materials);
            materials_changed = true;
        }
    }

    if (field_mask & kFieldParams)
    {
        if (const ncl_entity::ArrayStringIdFloatPairsDelta* params = ParamsOf(fields))
        {
            if (!ApplyParams(*params, dressed.params))
            {
                return;
            }

            params_changed = true;
        }
    }

    if (previous_index != dressed.entity_index || materials_changed)
    {
        if (!ReplaceOwner(entity_id, dressed))
        {
            return;
        }
    }
    else if (params_changed && !ReplaceOwnerParams(entity_id, dressed))
    {
        return;
    }

    if (previous_index != dressed.entity_index)
    {
        RemoveOwner(entity_id, previous_index);
    }

    it->second = std::move(dressed);
}

void MaterialSyncSystem::OnDestroy(uint16_t entity_id)
{
    auto it = entities_.find(entity_id);

    if (it == entities_.end())
    {
        return;
    }

    RemoveOwner(entity_id, it->second.entity_index);
    entities_.erase(it);
}

void MaterialSyncSystem::OnClear()
{
    for (const auto& [entity_id, dressed] : entities_)
    {
        RemoveOwner(entity_id, dressed.entity_index);
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
