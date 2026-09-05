#include "view/view_bob.h"

#include <algorithm>
#include <cmath>

#include <ncl_math/scalar.h>
#include <ncl_math/vec3.h>

namespace
{
    // Coefficients the offsets reach the view model through.
    constexpr float kClassicForwardScale = 0.4f;
    constexpr float kClassicSwayPitchScale = 0.3f;
    constexpr float kClassicSwayYawScale = 0.5f;
    constexpr float kClassicSwayRollScale = 1.0f;
    constexpr float kModernForwardScale = 0.4f;
    constexpr float kModernUpScale = 0.1f;
    constexpr float kModernSideScale = 0.2f;

    float BobPhase(double time, float cycle_len)
    {
        float cycle = static_cast<float>(time - static_cast<int>(time / cycle_len) * cycle_len) / cycle_len;

        if (!(cycle >= 0.0f && cycle < 1.0f))
        {
            cycle = 0.0f;
        }

        return cycle;
    }

    float ClampBobFinite(float bob, float lo, float hi)
    {
        if (!std::isfinite(bob))
        {
            return 0.0f;
        }

        return std::clamp(bob, lo, hi);
    }

    float RaisedCycle(float cycle, float bob_up)
    {
        if (cycle < bob_up)
        {
            return ncl_math::kPi * cycle / bob_up;
        }

        return ncl_math::kPi + ncl_math::kPi * (cycle - bob_up) / (1.0f - bob_up);
    }
}

namespace view_bob
{
    float StepClassicBob(ClassicBobState& state, const BobParams& params, float frametime, float speed)
    {
        state.bob_time += frametime;

        if (params.bob_cycle <= 0.0f)
        {
            return 0.0f;
        }

        float cycle = RaisedCycle(BobPhase(state.bob_time, params.bob_cycle), params.bob_up);

        float bob = speed * params.bob;
        bob = bob * 0.3f + bob * 0.7f * sinf(cycle);

        return ClampBobFinite(bob, -7.0f, 4.0f);
    }

    ModernBobOffsets StepModernBob(ModernBobState& state, const BobParams& params, float time, float speed, bool onground)
    {
        float max_speed_delta = std::max(0.0f, (time - state.last_bob_time) * kModernSpeedAccel);

        speed = std::clamp(speed, state.last_speed - max_speed_delta, state.last_speed + max_speed_delta);
        speed = std::clamp(speed, -320.0f, 320.0f);

        state.last_speed = speed;

        float lower_amt = params.lower_amt * (speed * 0.001f);

        float bob_offset = ncl_math::Remap(speed, 0, 320, 0, 1);

        state.bob_time += (time - state.last_bob_time) * bob_offset;
        state.last_bob_time = time;

        /* scale the bob by 1.25, this wasn't in 10040 but this way
        cs 1.6's default cl_bobcycle value (0.8) will look right */
        float bob_cycle = (((1000.0f - 150.0f) / 3.5f) * 0.001f) * params.bob_cycle * 1.25f;

        if (bob_cycle <= 0.0f)
        {
            return ModernBobOffsets{};
        }

        float cycle = RaisedCycle(BobPhase(state.bob_time, bob_cycle), params.bob_up);

        float bob_scale = onground ? 0.00625f : 0.00125f;

        float vert = speed * (bob_scale * params.amt_vert);
        vert = vert * 0.3f + vert * 0.7f * sinf(cycle);
        vert = ClampBobFinite(vert - lower_amt, -8.0f, 4.0f);

        // CSGO quirk kept verbatim: (int)(t / c * 2) truncates half-cycles, not t / (2c) cycles,
        // so this is intentionally not BobPhase(bob_time, bob_cycle * 2)
        cycle = state.bob_time - (int)(state.bob_time / bob_cycle * 2) * bob_cycle * 2;
        cycle /= bob_cycle * 2;
        cycle = RaisedCycle(cycle, params.bob_up);

        float hor = speed * (bob_scale * params.amt_lat);
        hor = hor * 0.3f + hor * 0.7f * sinf(cycle);
        hor = ClampBobFinite(hor, -7.0f, 4.0f);

        return ModernBobOffsets{vert, hor};
    }

    BobOffsets PlaceClassicBob(float bob, int style)
    {
        BobOffsets offsets{};
        offsets.forward = bob * kClassicForwardScale;

        if (style == kStyleClassicSway)
        {
            offsets.pitch = -bob * kClassicSwayPitchScale;
            offsets.yaw = -bob * kClassicSwayYawScale;
            offsets.roll = -bob * kClassicSwayRollScale;
        }

        return offsets;
    }

    BobOffsets PlaceModernBob(const ModernBobOffsets& offsets)
    {
        BobOffsets placed{};
        placed.forward = offsets.vert * kModernForwardScale;
        placed.up = offsets.vert * kModernUpScale;
        placed.side = offsets.hor * kModernSideScale;

        return placed;
    }
}
