#include "RadarLabel.h"

#include "main.h"

#include <ncl_math/scalar.h>

#include "RadarDraw.h"

namespace
{
    // kFlashFadeStart stays below kLabelFlashHide so the ramp is already under way where
    // the unbatched path cuts.
    constexpr float kFlashFadeStart = 90.0f;
    constexpr float kFlashFadeEnd = 200.0f;
} // namespace

float RadarLabel_ComputeFlashFade(int flash_alpha)
{
    return 1.0f - ncl_math::SmoothStep(kFlashFadeStart, kFlashFadeEnd, (float)flash_alpha);
}

void RadarLabel_Draw(const char* text, int x, int y, ncl_math::Color color, int flash_alpha)
{
    // A hard cut, not a ramp: this font's drop shadow is baked black into the glyph and
    // cannot be faded. Not the additive shadow-free engine font instead, which is a much
    // larger, non-scaling face.
    if (flash_alpha >= kLabelFlashHide)
    {
        return;
    }

    ncl_math::ColorF faded = RadarDraw_ApplyFade(color).ToFloat();
    gEngfuncs.pfnDrawSetTextColor(faded.r, faded.g, faded.b);
    gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(text));
}
