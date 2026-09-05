#pragma once

#include <cmath>

// Scalar math and easing helpers. The interpolation primitives here are the
// building blocks for the vector and color overloads in vec3.h / color.h.
namespace ncl_math
{
    // Length below which a vector is treated as directionless. Sized for
    // unit-scale magnitudes; data in larger units needs its own threshold.
    inline constexpr float kNearZero = 1e-3f;

    inline float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    inline float Clamp(float v, float lo, float hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    inline float Saturate(float v)
    {
        return Clamp(v, 0.0f, 1.0f);
    }

    // Fraction of [a, b] at which v sits (unclamped): the inverse of Lerp.
    inline float InvLerp(float a, float b, float v)
    {
        return (v - a) / (b - a);
    }

    inline float Remap(float v, float in_lo, float in_hi, float out_lo, float out_hi)
    {
        return out_lo + (v - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
    }

    inline float SmoothStep(float edge0, float edge1, float x)
    {
        float t = Saturate((x - edge0) / (edge1 - edge0));
        return t * t * (3.0f - 2.0f * t);
    }

    // Per-frame weight for frame-rate-independent exponential smoothing.
    inline float SmoothingFactor(float rate, float dt)
    {
        return 1.0f - std::exp(-rate * dt);
    }

    // Moves current toward target by this frame's exponential-smoothing weight.
    inline float ExpSmooth(float current, float target, float rate, float dt)
    {
        return Lerp(current, target, SmoothingFactor(rate, dt));
    }

    // Shortest-path interpolation between two angles in degrees.
    inline float LerpAngle(float a, float b, float t)
    {
        float d = std::fmod(b - a, 360.0f);

        if (d < -180.0f)
        {
            d += 360.0f;
        }
        else if (d > 180.0f)
        {
            d -= 360.0f;
        }

        return a + t * d;
    }

    // Hold-then-linear fade: 1 while elapsed <= hold, 0 at/after fade_end, linear
    // between.
    inline float HoldLinear(float elapsed, float hold, float fade_end)
    {
        if (elapsed <= hold)
        {
            return 1.0f;
        }

        if (elapsed >= fade_end)
        {
            return 0.0f;
        }

        return 1.0f - (elapsed - hold) / (fade_end - hold);
    }
} // namespace ncl_math
