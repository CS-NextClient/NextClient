#include "RadarLabel.h"
#include "RadarDraw.h"
#include "../../main.h"

#include <ncl_math/scalar.h>

namespace
{
    // Flash strength at which the label starts fading, and where it has fully vanished. The lower
    // edge sits below kLabelFlashHide so the ramp is under way by the point the old hard cut fired.
    constexpr float kFlashFadeStart = 90.0f;
    constexpr float kFlashFadeEnd = 200.0f;
} // namespace

float RadarLabel_FlashFade(int flash_alpha)
{
    return 1.0f - ncl_math::SmoothStep(kFlashFadeStart, kFlashFadeEnd, (float)flash_alpha);
}

void RadarLabel_Draw(const char* text, int x, int y, ncl_math::Color color, int flash_alpha)
{
    // One renderer for every state: the console font (DrawConsoleString), so the
    // label size never changes. Its drop shadow is baked black into the glyph
    // texture and modulated by the vertex colour, so it cannot be recoloured by
    // tinting the text, and the only additive (shadow-free) engine font is a much
    // larger non-scaling face. Instead, tint the glyph toward the flash colour so
    // it washes out, and once the flash is bright enough that the black shadow would
    // stand out against the whitened radar, stop drawing the label entirely. The
    // flash spikes then decays, so the label vanishes during the blinding peak and
    // washes back in as sight returns.
    if (flash_alpha >= kLabelFlashHide)
    {
        return;
    }

    ncl_math::ColorF faded = RadarDraw_FadeColor(color).ToFloat();
    gEngfuncs.pfnDrawSetTextColor(faded.r, faded.g, faded.b);
    gEngfuncs.pfnDrawConsoleString(x, y, const_cast<char*>(text));
}
