#pragma once

#include <memory>

#include "HudAmmo.h"
#include "HudHealth.h"
#include "HudCrosshair.h"
#include "HudDamageIcons.h"
#include "HudDamageDirection.h"
#include "HudRadar.h"
#include "HudDeathNotice.h"
#include "hud_sprite/hud_sprite_store.h"

class GameHud
{
    std::shared_ptr<HudHealth> health_;
    std::shared_ptr<HudCrosshair> crosshair_;
    std::shared_ptr<HudSpriteStore> sprite_store_;
    std::shared_ptr<HudDamageIcons> damage_icons_;
    std::shared_ptr<HudDamageDirection> damage_direction_;
    std::shared_ptr<HudRadar> radar_;
    std::shared_ptr<HudDeathNotice> death_notice_;
    std::shared_ptr<HudAmmo> ammo_;

    std::vector<std::shared_ptr<HudBase>> all_hud_;

public:
    explicit GameHud(nitroapi::NitroApiInterface* nitro_api);

    void Init();
    void VidInit();
    void Draw(float time);
    void Think(float time);
    void Reset();
    void InitHUDData();		// called every time a server is connected to

    [[nodiscard]] std::shared_ptr<HudSpriteStore> get_sprite_store() const { return sprite_store_; }
    [[nodiscard]] std::shared_ptr<HudDeathNotice> get_deathnotice() const { return death_notice_; }

private:
    void FetchCvars();

public:
    cvar_t* cl_righthand;
};