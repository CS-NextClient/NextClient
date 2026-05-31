#include "PlayerSyncSystem.h"

#include <format>

#include <tier0/basetypes.h>
#include <const.h>

#include "cl_ncl_entity_sync.h"

namespace
{
    constexpr uint16_t FIELD_PLAYER_ID = (1u << 0);
    constexpr uint16_t FIELD_ACTIVE_WEAPON_ID = (1u << 1);

    std::vector<ClientNclEntityFieldDescriptor> g_fields = {
        {FIELD_PLAYER_ID, ncl_entity::FieldType::BYTE},
        {FIELD_ACTIVE_WEAPON_ID, ncl_entity::FieldType::UINT},
    };
} // namespace

const std::vector<ClientNclEntityFieldDescriptor>& PlayerSyncSystem::Fields() const
{
    return g_fields;
}

void PlayerSyncSystem::OnCreate(uint16_t entity_id, const NclEntityFields& fields)
{
    auto player_it = fields.find(FIELD_PLAYER_ID);
    auto weapon_it = fields.find(FIELD_ACTIVE_WEAPON_ID);

    if (player_it != fields.end() && weapon_it != fields.end())
    {
        uint8_t player_id = std::get<uint8_t>(player_it->second.value);
        uint32_t active_weapon_id = std::get<uint32_t>(weapon_it->second.value);

        if (player_id >= 1 && player_id <= (uint8_t)MAX_CLIENTS)
        {
            auto existing = entity_to_player_.find(entity_id);
            if (existing != entity_to_player_.end())
            {
                auto old_rev = player_id_to_entity_id_.find(existing->second.player_id);
                if (old_rev != player_id_to_entity_id_.end() && old_rev->second == entity_id)
                {
                    player_id_to_entity_id_.erase(old_rev);
                }
            }

            player_id_to_entity_id_[player_id] = entity_id;
            entity_to_player_[entity_id] = {player_id, active_weapon_id};
        }
    }
}

void PlayerSyncSystem::OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields)
{
    auto entity_it = entity_to_player_.find(entity_id);
    if (entity_it == entity_to_player_.end())
    {
        return;
    }

    if (field_mask & FIELD_PLAYER_ID)
    {
        auto it = fields.find(FIELD_PLAYER_ID);
        if (it != fields.end())
        {
            uint8_t new_player_id = std::get<uint8_t>(it->second.value);

            if (new_player_id >= 1 && new_player_id <= (uint8_t)MAX_CLIENTS)
            {
                auto old_rev = player_id_to_entity_id_.find(entity_it->second.player_id);
                if (old_rev != player_id_to_entity_id_.end() && old_rev->second == entity_id)
                {
                    player_id_to_entity_id_.erase(old_rev);
                }

                player_id_to_entity_id_[new_player_id] = entity_id;
                entity_it->second.player_id = new_player_id;
            }
        }
    }

    if (field_mask & FIELD_ACTIVE_WEAPON_ID)
    {
        auto it = fields.find(FIELD_ACTIVE_WEAPON_ID);
        if (it != fields.end())
        {
            uint32_t new_active_weapon_id = std::get<uint32_t>(it->second.value);

            entity_it->second.active_weapon_id = new_active_weapon_id;
        }
    }
}

void PlayerSyncSystem::OnDestroy(uint16_t entity_id)
{
    auto it = entity_to_player_.find(entity_id);
    if (it == entity_to_player_.end())
    {
        return;
    }

    auto rev = player_id_to_entity_id_.find(it->second.player_id);
    if (rev != player_id_to_entity_id_.end() && rev->second == entity_id)
    {
        player_id_to_entity_id_.erase(rev);
    }
    entity_to_player_.erase(it);
}

void PlayerSyncSystem::OnClear()
{
    player_id_to_entity_id_.clear();
    entity_to_player_.clear();
}

void PlayerSyncSystem::CollectDebugInfo(std::vector<std::string>& out_lines)
{
    out_lines.push_back("");
    out_lines.push_back("System: PlayerSync");
    
    for (const auto& [entity_id, player] : entity_to_player_)
    {
        out_lines.push_back(std::format("   player={}", player.player_id));
        out_lines.push_back(std::format("      active weapon={}", player.active_weapon_id));
    }
}

int PlayerSyncSystem::GetPlayerActiveWeapon(int player_id)
{
    if (player_id < 1 || player_id > MAX_CLIENTS)
    {
        return 0;
    }

    auto it = player_id_to_entity_id_.find(player_id);
    if (it == player_id_to_entity_id_.end())
    {
        return 0;
    }

    auto entity_it = entity_to_player_.find(it->second);
    if (entity_it == entity_to_player_.end())
    {
        return 0;
    }

    return entity_it->second.active_weapon_id;
}
