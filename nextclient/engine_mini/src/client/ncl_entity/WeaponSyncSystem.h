#pragma once
#include "engine.h"

#include <vector>

#include <ankerl/unordered_dense.h>

#include "INclEntityHandler.h"

class WeaponSyncSystem : public INclEntityHandler
{
    struct WeaponData
    {
        uint32_t weapon_id;
        ankerl::unordered_dense::map<sfx_t*, sfx_t*> sound_overrides;
    };

    ankerl::unordered_dense::map<uint16_t, WeaponData> entity_to_weapon_;
    ankerl::unordered_dense::map<uint32_t, uint16_t> weapon_id_to_entity_id_;

public:
    const std::vector<ClientNclEntityFieldDescriptor>& Fields() const override;
    void OnCreate(uint16_t entity_id, const NclEntityFields& fields) override;
    void OnUpdate(uint16_t entity_id, uint16_t field_mask, const NclEntityFields& fields) override;
    void OnDestroy(uint16_t entity_id) override;
    void OnClear() override;
    void CollectDebugInfo(std::vector<std::string>& out_lines) override;

    const ankerl::unordered_dense::map<sfx_t*, sfx_t*>& GetOverrides(uint32_t weapon_id) const;

private:
    void ApplyDelta(const ncl_entity::ArrayStringIdPairsDelta& delta, ankerl::unordered_dense::map<sfx_t*, sfx_t*>& overrides);
};
