#pragma once

// extra_player_info_t::teamnumber, typed. The values are the SDK's TEAM_* (cdll_dll.h),
// which HudRadarAdvanced static_asserts. A mod is free to send a number outside the set;
// it then compares unequal to every enumerator, which is what the call sites want.
enum class RadarTeam : int
{
    Unassigned = 0,
    Terrorist = 1,
    CounterTerrorist = 2,
    Spectator = 3,
};

inline RadarTeam RadarTeam_From(int teamnumber)
{
    return static_cast<RadarTeam>(teamnumber);
}
