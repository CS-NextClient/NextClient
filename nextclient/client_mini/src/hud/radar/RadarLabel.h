#pragma once

#include <ncl_math/color.h>

// Above this flash strength the radar is washed bright enough that a label reads as a
// dark smear rather than text, so labels are skipped entirely. Shared with the batched
// glyph path so both label renderers disappear at the same point.
inline constexpr int kLabelFlashHide = 120;

// Label alpha multiplier for the current flash, ramped smoothly to zero over the blinding
// peak instead of the hard cut kLabelFlashHide forces. Only the batched glyph path can use
// this: it fades through the prim batch's per-vertex alpha, where the console font's baked
// black shadow cannot be faded and must be cut.
float RadarLabel_FlashFade(int flash_alpha);

// Console-font HUD label with the radar's flashbang behaviour: the glyph colour
// washes toward the active fade, and above a threshold the label is skipped
// entirely. flash_alpha is the current blinding-flash alpha (0 = no flash).
void RadarLabel_Draw(const char* text, int x, int y, ncl_math::Color color, int flash_alpha);
