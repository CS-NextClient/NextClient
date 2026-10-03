#include "ServerMarker.h"

#include <algorithm>

#include <ncl_math/scalar.h>

#include "RadarDraw.h"
#include "RadarPalette.h"

namespace
{
    constexpr float kEaseRate = 16.0f; // 1/s
    constexpr float kSnapDist = 200.0f; // world units

    // A longer frame eases no further, so a hitch does not turn the ease into a jump.
    constexpr float kMaxEaseStep = 0.1f; // seconds
} // namespace

void ServerMarker_DrawShape(ncl_math::Vector2 screen, int size)
{
    ncl_math::Color col = RadarPalette::kServerMarker;
    RadarDraw_DrawRing(screen, (float)size + 1.5f, (float)size - 0.5f, col);
    RadarDraw_DrawCross(screen, size * 0.75f, 1.5f, col);
}

ncl_math::Vector2 ServerMarker_Ease(ncl_math::Vector2 pos, ncl_math::Vector2 target, float dt)
{
    ncl_math::Vector2 dir = target - pos;

    if (dir.LengthSqr() > kSnapDist * kSnapDist)
    {
        return target;
    }

    return pos + dir * ncl_math::SmoothingFactor(kEaseRate, std::clamp(dt, 0.0f, kMaxEaseStep));
}
