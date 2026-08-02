#pragma once

#include <ncl_math/color.h>

// Above this flash strength the radar is washed bright enough that a label reads as a
// dark smear rather than text, so labels are skipped entirely.
inline constexpr int kLabelFlashHide = 120;

// A smooth ramp instead of the kLabelFlashHide cut. Only the batched glyph path can use
// it: it fades through the prim batch's per-vertex alpha, where the console font's baked
// black shadow cannot be faded and must be cut.
float RadarLabel_ComputeFlashFade(int flash_alpha);

inline float RadarLabel_ComputeFlashFadeFor(bool batched, int flash_alpha)
{
    return batched ? RadarLabel_ComputeFlashFade(flash_alpha) : (flash_alpha >= kLabelFlashHide ? 0.0f : 1.0f);
}

// flash_alpha is 0 when there is no flash.
void RadarLabel_Draw(const char* text, int x, int y, ncl_math::Color color, int flash_alpha);
