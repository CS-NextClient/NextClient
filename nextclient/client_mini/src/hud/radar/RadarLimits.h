#pragma once

// Engine bounds this directory has to mirror.

// From the client SDK's hud.h, which the engine-free half of this directory (and the unit
// tests that compile it) cannot include; HudRadarAdvanced static_asserts both against the SDK.
inline constexpr int kRadarMaxPlayers = 64;
inline constexpr int kRadarMaxHostages = 24;

// The largest set any feature feeds through MarkerFeed, which is the player one.
inline constexpr int kRadarMaxMarkers = kRadarMaxPlayers;

// No header exposes this one: COM_ParseFile writes up to the engine's own token size, and a
// smaller buffer is smashed by long worldspawn values (the de_inferno "wad" key).
inline constexpr int kComParseTokenSize = 2048;
