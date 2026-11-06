#pragma once

#include <algorithm>

#include <cvars/CvarDefinition.h>
#include <goldsrc/cvar.h>

enum class RadarMapMode
{
    None = 0,
    OverviewTextured = 1,
    OverviewFlat = 2,
    RuntimeTextured = 3,
    RuntimeFlat = 4,
    SliceTextured = 5,
    SliceFlat = 6,
};

struct RadarMapModeDef
{
    RadarMapMode mode;
    const char* label;
};

inline constexpr RadarMapModeDef kRadarMapModeDefs[] = {
    {RadarMapMode::OverviewTextured, "#GameUI_HudRadar_MapMode_Overview"},
    {RadarMapMode::SliceFlat, "#GameUI_HudRadar_MapMode_GeneratedFlat"},
    {RadarMapMode::SliceTextured, "#GameUI_HudRadar_MapMode_GeneratedTextured"},
};

// Never returns None.
constexpr RadarMapMode RadarMapMode_FromCvar(float value)
{
    int mode =
        std::clamp(static_cast<int>(value), static_cast<int>(RadarMapMode::OverviewTextured), static_cast<int>(RadarMapMode::SliceFlat));

    if (mode == static_cast<int>(RadarMapMode::OverviewFlat))
    {
        return RadarMapMode::OverviewTextured;
    }

    if (mode == static_cast<int>(RadarMapMode::RuntimeTextured))
    {
        return RadarMapMode::SliceTextured;
    }

    if (mode == static_cast<int>(RadarMapMode::RuntimeFlat))
    {
        return RadarMapMode::SliceFlat;
    }

    return static_cast<RadarMapMode>(mode);
}

namespace radar
{
    inline constexpr cvars::CvarDefinition kCvarDefinitions[] = {
        {"hud_radar_mode", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_scale", "1.15", 0.5f, 3.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_map_scale", "0.8", 0.25f, 3.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_map_shift", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_opacity", "100", 0.0f, 100.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_round", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_fullmap", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_shift_scoreboard", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_rotate", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_names", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_health", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_tracks", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_map_mode", "5", 1.0f, 6.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_map_mask", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_map_smoothing", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_bg", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_health_size", "1.0", 0.3f, 4.0f, goldsrc::FCVAR_ARCHIVE},
        {"hud_radar_name_size", "0.8", 0.6f, 2.0f, goldsrc::FCVAR_ARCHIVE},
        // 1: show the teammates the server reveals, 0: gate teammates by line of sight like enemies.
        {"hud_radar_allies", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
    };

    constexpr cvars::CvarDefinition FindCvar(std::string_view name)
    {
        return cvars::FindDefinition(kCvarDefinitions, name);
    }
} // namespace radar
