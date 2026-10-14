#include "GameHud.h"
#if NEXTCLIENT_WITH_PLUGINS
#include <nextclient/runtime.h>
#endif
#include <goldsrc/client_api.h>

#include "hud/radar/integration/RadarConfig.h"
#include "hud/radar/integration/RadarDebug.h"

extern goldsrc::cl_enginefunc_t gEngfuncs;

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

    radar_advanced_ = std::make_shared<HudRadarAdvanced>(nitro_api);
    all_hud_.push_back(radar_advanced_);

    death_notice_ = std::make_shared<HudDeathNotice>(nitro_api);
    all_hud_.push_back(death_notice_);

    ammo_ = std::make_shared<HudAmmo>(nitro_api);
    all_hud_.push_back(ammo_);
}

void GameHud::Init()
{
    FetchCvars();

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
    for (auto& item : all_hud_) {
        if (item == radar_advanced_)
        {
            continue; // drawn separately via DrawRadar
        }
#if NEXTCLIENT_WITH_PLUGINS
        if ((item == health_ && nc_runtime_ui_hidden(NC_UI_HEALTH)) ||
            (item == radar_ && nc_runtime_ui_hidden(NC_UI_RADAR)) ||
            (item == death_notice_ && nc_runtime_ui_hidden(NC_UI_DEATH_NOTICES)))
        {
            continue;
        }
#endif
        item->Draw(time);
    }
}

void GameHud::PrepareRadar()
{
#if NEXTCLIENT_WITH_PLUGINS
    if (nc_runtime_ui_hidden(NC_UI_RADAR))
    {
        return;
    }
#endif
    radar_advanced_->PrepareHudTextZone();
}

void GameHud::UpdateRadarScoreboard(bool hud_visible)
{
#if NEXTCLIENT_WITH_PLUGINS
    hud_visible = hud_visible && !nc_runtime_ui_hidden(NC_UI_RADAR);
#endif
    radar_advanced_->UpdateScoreboard(hud_visible);
}

void GameHud::DrawRadar(float time)
{
#if NEXTCLIENT_WITH_PLUGINS
    if (nc_runtime_ui_hidden(NC_UI_RADAR))
    {
        return;
    }
#endif
    radar_advanced_->Draw(time);
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

void GameHud::FetchCvars()
{
    cl_righthand = gEngfuncs.pfnGetCvarPointer("cl_righthand");

    RadarConfig_Init();
    RadarDebug_Init();
}
