#include "RadarFade.h"

#include <algorithm>

#include <goldsrc/view.h>

namespace
{
    // FFADE_* (engine shake.h); only the bits used by V_FadeAlpha are needed.
    constexpr int kFadeOut = 0x0001;
    constexpr int kFadeStayout = 0x0004;
} // namespace

int RadarFade_ComputeAlpha(const goldsrc::screenfade_t& sf, double time)
{
    if (sf.fadeReset < time && sf.fadeEnd < time && !(sf.fadeFlags & kFadeStayout))
    {
        return 0;
    }

    int alpha;

    if (!(sf.fadeFlags & kFadeStayout))
    {
        alpha = (int)(sf.fadeSpeed * (sf.fadeEnd - time));

        if (sf.fadeFlags & kFadeOut)
        {
            alpha += sf.fadealpha;
        }
    }
    else
    {
        alpha = sf.fadealpha;

        if ((sf.fadeFlags & kFadeOut) && sf.fadeTotalEnd > time)
        {
            alpha += (int)((sf.fadeTotalEnd - time) * sf.fadeSpeed);
        }
    }

    alpha = std::min(alpha, (int)sf.fadealpha);

    return std::max(0, alpha);
}

bool RadarFade_IsNearWhite(const goldsrc::screenfade_t& sf)
{
    bool result = sf.fader >= 200 && sf.fadeg >= 200 && sf.fadeb >= 200;
    return result;
}
