#include "engine.h"
#include "WeaponSyncSystem.h"

#include <format>

#include "PlayerSyncSystem.h"
#include "cl_ncl_entity_sync.h"
#include "client/cl_string_registry.h"
#include "console/console.h"

namespace
{
    constexpr uint16_t FIELD_WEAPON_ID = 1u << 0;
    constexpr uint16_t FIELD_SOUND_OVERRIDES = 1u << 1;

    std::vector<ClientNclEntityFieldDescriptor> g_Fields = {
        {FIELD_WEAPON_ID, ncl_entity::FieldType::UINT},
        {FIELD_SOUND_OVERRIDES, ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA},
    };

    sfx_t* PrecacheReplacement(const char* name)
    {
        sfx_t* sfx = eng()->S_PrecacheSound(const_cast<char*>(name));
        if (!sfx)
        {
            Con_DPrintf(ConLogType::Warning, "WeaponSoundOverride: failed to precache replacement sound \"%s\"\n", name);
        }
        return sfx;
    }
} // namespace

const std::vector<ClientNclEntityFieldDescriptor>& WeaponSyncSystem::Fields() const
{
    return g_Fields;
}

void WeaponSyncSystem::OnCreate(uint16_t entity_id, const NclEntityFields& fields)
{
    auto weapon_it = fields.find(FIELD_WEAPON_ID);
    auto sound_it = fields.find(FIELD_SOUND_OVERRIDES);

    if (weapon_it != fields.end() && sound_it != fields.end())
    {
        uint32_t active_weapon_id = std::get<uint32_t>(weapon_it->second.value);
        const auto& delta = std::get<ncl_entity::ArrayStringIdPairsDelta>(sound_it->second.value);

        auto existing = entity_to_weapon_.find(entity_id);
        if (existing != entity_to_weapon_.end())
        {
            auto old_rev = weapon_id_to_entity_id_.find(existing->second.weapon_id);
            if (old_rev != weapon_id_to_entity_id_.end() && old_rev->second == entity_id)
            {
                weapon_id_to_entity_id_.erase(old_rev);
            }
        }

        WeaponData weapon_data;
        weapon_data.weapon_id = active_weapon_id;
        ApplyDelta(delta, weapon_data.sound_overrides);

        weapon_id_to_entity_id_[active_weapon_id] = entity_id;
        entity_to_weapon_[entity_id] = std::move(weapon_data);
    }
}

void WeaponSyncSystem::OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields)
{
    auto entity_it = entity_to_weapon_.find(entity_id);
    if (entity_it == entity_to_weapon_.end())
    {
        return;
    }

    if (field_mask & FIELD_WEAPON_ID)
    {
        auto it = fields.find(FIELD_WEAPON_ID);
        if (it != fields.end())
        {
            uint32_t new_weapon_id = std::get<uint32_t>(it->second.value);

            auto old_rev = weapon_id_to_entity_id_.find(entity_it->second.weapon_id);
            if (old_rev != weapon_id_to_entity_id_.end() && old_rev->second == entity_id)
            {
                weapon_id_to_entity_id_.erase(old_rev);
            }

            weapon_id_to_entity_id_[new_weapon_id] = entity_id;
            entity_it->second.weapon_id = new_weapon_id;
        }
    }

    if (field_mask & FIELD_SOUND_OVERRIDES)
    {
        auto it = fields.find(FIELD_SOUND_OVERRIDES);
        if (it != fields.end())
        {
            const auto& delta = std::get<ncl_entity::ArrayStringIdPairsDelta>(it->second.value);

            ApplyDelta(delta, entity_it->second.sound_overrides);
        }
    }
}

void WeaponSyncSystem::OnDestroy(uint16_t entity_id)
{
    auto it = entity_to_weapon_.find(entity_id);
    if (it == entity_to_weapon_.end())
    {
        return;
    }

    auto rev = weapon_id_to_entity_id_.find(it->second.weapon_id);
    if (rev != weapon_id_to_entity_id_.end() && rev->second == entity_id)
    {
        weapon_id_to_entity_id_.erase(rev);
    }
    entity_to_weapon_.erase(it);
}

void WeaponSyncSystem::OnClear()
{
    weapon_id_to_entity_id_.clear();
    entity_to_weapon_.clear();
}

void WeaponSyncSystem::CollectDebugInfo(std::vector<std::string>& out_lines)
{
    out_lines.push_back("");
    out_lines.push_back("System: WeaponSync");

    for (const auto& [entity_id, weapon_data] : entity_to_weapon_)
    {
        out_lines.push_back(std::format("   weapon={}", weapon_data.weapon_id));
        out_lines.push_back("      sound overrides");
        
        for (const auto& [original, replacement] : weapon_data.sound_overrides)
        {
            out_lines.push_back(std::format("         {} -> {}", original->name, replacement->name));
        }
    }
}

const ankerl::unordered_dense::map<sfx_t*, sfx_t*>& WeaponSyncSystem::GetOverrides(uint32_t weapon_id) const
{
    static ankerl::unordered_dense::map<sfx_t*, sfx_t*> empty;

    auto it = weapon_id_to_entity_id_.find(weapon_id);
    if (it == weapon_id_to_entity_id_.end())
    {
        return empty;
    }

    auto entity_it = entity_to_weapon_.find(it->second);
    if (entity_it == entity_to_weapon_.end())
    {
        return empty;
    }

    return entity_it->second.sound_overrides;
}

void WeaponSyncSystem::ApplyDelta(const ncl_entity::ArrayStringIdPairsDelta& delta, ankerl::unordered_dense::map<sfx_t*, sfx_t*>& overrides)
{
    if (delta.clear)
    {
        overrides.clear();
        return;
    }

    for (const auto& pair : delta.removed)
    {
        const char* original_path = CL_StringRegistryGetString(pair.first);
        if (!original_path)
        {
            continue;
        }

        sfx_t* original_sfx = eng()->S_FindName(original_path, nullptr);
        if (original_sfx)
        {
            overrides.erase(original_sfx);
        }
    }

    for (const auto& pair : delta.added)
    {
        const char* original_path = CL_StringRegistryGetString(pair.first);
        const char* replacement_path = CL_StringRegistryGetString(pair.second);

        if (!original_path || !replacement_path)
        {
            continue;
        }

        sfx_t* original_sfx = eng()->S_FindName(original_path, nullptr);
        if (!original_sfx)
        {
            Con_DPrintf(ConLogType::Warning, "WeaponSoundOverride: unknown original sound \"%s\"\n", original_path);
            continue;
        }

        sfx_t* replacement_sfx = PrecacheReplacement(replacement_path);
        if (!replacement_sfx)
        {
            Con_DPrintf(ConLogType::Warning, "Can't precache replacement sound: \"%s\"\n", replacement_path);
            continue;
        }

        overrides[original_sfx] = replacement_sfx;
    }
}
