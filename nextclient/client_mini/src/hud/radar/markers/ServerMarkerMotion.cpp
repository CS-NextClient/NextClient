#include "ServerMarker.h"

#include <algorithm>

#include <ncl_math/scalar.h>

namespace
{
    constexpr float kEaseRate = 16.0f; // 1/s
    constexpr float kSnapDist = 200.0f; // world units

    // A longer frame eases no further, so a hitch does not turn the ease into a jump.
    constexpr float kMaxEaseStep = 0.1f; // seconds
} // namespace

ncl_math::Vector3 ServerMarker_Ease(ncl_math::Vector3 pos, ncl_math::Vector3 target, float dt)
{
    ncl_math::Vector3 dir = target - pos;

    if (dir.LengthSqr() > kSnapDist * kSnapDist)
    {
        return target;
    }

    return pos + dir * ncl_math::SmoothingFactor(kEaseRate, std::clamp(dt, 0.0f, kMaxEaseStep));
}
