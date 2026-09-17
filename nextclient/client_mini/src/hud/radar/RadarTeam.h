#pragma once

// goldsrc::cstrike::extra_player_info_t::teamnumber, typed; the enumerators equal
// goldsrc::cstrike::TEAM_* of goldsrc/cstrike/game.h. A mod may send a number outside
// the set, which then compares unequal to every enumerator.
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
