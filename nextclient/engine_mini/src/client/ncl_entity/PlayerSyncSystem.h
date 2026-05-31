#pragma once
#include <cstdint>

#include <ankerl/unordered_dense.h>

#include "INclEntityHandler.h"

class PlayerSyncSystem : public INclEntityHandler
{
    struct PlayerData
    {
        uint8_t player_id;
        uint32_t active_weapon_id;
    };

    ankerl::unordered_dense::map<uint16_t, PlayerData> entity_to_player_;
    ankerl::unordered_dense::map<uint8_t, uint16_t> player_id_to_entity_id_;

public:
    const std::vector<ClientNclEntityFieldDescriptor>& Fields() const override;
    void OnCreate(uint16_t entity_id, const NclEntityFields& fields) override;
    void OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields) override;
    void OnDestroy(uint16_t entity_id) override;
    void OnClear() override;
    void CollectDebugInfo(std::vector<std::string>& out_lines) override;

    int GetPlayerActiveWeapon(int player_id);
};
