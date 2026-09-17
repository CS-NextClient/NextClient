#pragma once

#include <goldsrc/cstrike/hud.h>

// Engine bounds this directory has to mirror. Literals rather than aliases of the goldsrc
// constants: a change on the engine side has to stop the build, not silently resize the tables.

inline constexpr int kRadarMaxPlayers = 64;
inline constexpr int kRadarMaxHostages = 24;

static_assert(kRadarMaxPlayers == goldsrc::cstrike::MAX_PLAYERS, "player table size mismatch");
static_assert(kRadarMaxHostages == goldsrc::cstrike::MAX_HOSTAGES, "hostage table size mismatch");

// The largest set any feature feeds through MarkerFeed, which is the player one.
inline constexpr int kRadarMaxMarkers = kRadarMaxPlayers;

// No header exposes this one: COM_ParseFile writes up to the engine's own token size, and a
// smaller buffer is smashed by long worldspawn values (the de_inferno "wad" key).
inline constexpr int kComParseTokenSize = 2048;
