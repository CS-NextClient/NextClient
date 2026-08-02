#pragma once

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

// Radar label text: decodes the nickname, lays it out against the glyph atlas and emits
// the cells into the radar's own primitive batch, so a label costs no engine text call.

// True when the batched path can be used at this pixel height; false leaves the caller on
// the engine text fallback.
bool RadarText_Available(int device_px);

// Pixel height of the current scheme's standard text font (the drop-shadowed console/chat face) for
// this resolution, so labels can size themselves relative to standard HUD text. Zero until the scheme
// font resolves (the caller then uses the engine-font fallback anyway).
int RadarText_StandardPx();

// Width in pixels of the laid-out string. slot identifies a stable caller (a player index)
// so the layout can be cached across frames; pass a negative slot to skip caching.
float RadarText_Width(int slot, const char* utf8, int device_px);

// Draws the string with its horizontal centre at pos.x and its top at pos.y, in the current scheme's
// standard text font (with its baked drop shadow), tinted by color.
void RadarText_DrawCentered(int slot, const char* utf8, ncl_math::Vector2 pos, int device_px, ncl_math::Color color);

void RadarText_ResetGl();

// CPU time spent shaping and emitting label text this frame, for the diagnostics overlay. The
// caller resets it at the start of each radar frame.
void RadarText_ResetStats();
float RadarText_Ms();
