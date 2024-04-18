#include "GameHud.h"

GameHud::GameHud(nitroapi::NitroApiInterface* nitro_api)
{
    health_ = std::make_shared<HudHealth>(nitro_api);
    all_hud_.push_back(health_);

    crosshair_ = std::make_shared<HudCrosshair>(nitro_api);
    all_hud_.push_back(crosshair_);

    sprite_store_ = std::make_shared<HudSpriteStore>();
    all_hud_.push_back(sprite_store_);

    damage_icons_ = std::make_shared<HudDamageIcons>(nitro_api);
    all_hud_.push_back(damage_icons_);

    damage_direction_ = std::make_shared<HudDamageDirection>(nitro_api);
    all_hud_.push_back(damage_direction_);

    radar_ = std::make_shared<HudRadar>(nitro_api);
    all_hud_.push_back(radar_);

    death_notice_ = std::make_shared<HudDeathNotice>(nitro_api);
    all_hud_.push_back(death_notice_);

    ammo_ = std::make_shared<HudAmmo>(nitro_api);
    all_hud_.push_back(ammo_);
}

void GameHud::Init()
{
    for (auto& item : all_hud_)
        item->Init();
}

void GameHud::VidInit()
{
    for (auto& item : all_hud_)
        item->VidInit();
}

void GameHud::Draw(float time)
{
    for (auto& item : all_hud_)
        item->Draw(time);
}

void GameHud::Think(float time)
{
    for (auto& item : all_hud_)
        item->Think(time);
}

void GameHud::Reset()
{
    for (auto& item : all_hud_)
        item->Reset();
}

void GameHud::InitHUDData()
{
    for (auto& item : all_hud_)
        item->InitHUDData();
}
