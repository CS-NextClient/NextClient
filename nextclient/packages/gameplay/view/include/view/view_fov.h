#pragma once

#include <cmath>

#include <ncl_math/vec3.h>

// The field the player looks through, as the client works it out for the engine: fov_angle
// offsets the 90 degrees the game hands down, within these bounds, and fov_horplus widens
// that field in proportion to how much wider than 4:3 the screen is.
namespace view_fov
{
    inline constexpr float kDefault = 90.0f;
    inline constexpr float kMin = 70.0f;
    inline constexpr float kMax = 100.0f;

    // fov is horizontal, in degrees, and comes back so; aspect is the screen's width over
    // its height. The widening keeps the vertical field a 4:3 screen would have.
    inline float ScreenFov(float fov, float aspect, bool horplus)
    {
        if (!horplus || aspect == 0.75f)
            return fov;

        return ncl_math::kRad2Deg * std::atan(std::tan(fov * ncl_math::kDeg2Rad / 2) * (aspect * 0.75f)) * 2;
    }
}
