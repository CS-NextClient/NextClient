#include <algorithm>
#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include <view/view_bob.h>

// The bob math as client_mini/src/view.cpp computed it before the package existed, kept
// verbatim apart from the cvars and ref_params becoming arguments: the package is a
// refactor of it and has to match bit for bit.
namespace reference
{
    constexpr float kPiF = static_cast<float>(3.14159265358979323846);

    struct BobVars
    {
        float bobTime;
        float lastBobTime;
        float lastSpeed;
        float vertBob;
        float horBob;
    };

    float Map(float value, float low1, float high1, float low2, float high2)
    {
        return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
    }

    float BobPhase(double time, float cycle_len)
    {
        float cycle = static_cast<float>(time - static_cast<int>(time / cycle_len) * cycle_len) / cycle_len;

        if (!(cycle >= 0.0f && cycle < 1.0f))
            cycle = 0.0f;

        return cycle;
    }

    float ClampBobFinite(float bob, float lo, float hi)
    {
        if (!std::isfinite(bob))
            return 0.0f;

        return std::clamp(bob, lo, hi);
    }

    void V_CalcBob_CSGO(BobVars& g_bobVars, const view_bob::BobParams& cvars, float time, float speed, bool onground)
    {
        float maxSpeedDelta;
        float lowerAmt;
        float bobOffset;
        float bobCycle;
        float bobScale;
        float cycle;

        maxSpeedDelta = std::max(0.f, (time - g_bobVars.lastBobTime) * 620.f);

        speed = std::clamp(speed, g_bobVars.lastSpeed - maxSpeedDelta, g_bobVars.lastSpeed + maxSpeedDelta);
        speed = std::clamp(speed, -320.f, 320.f);

        g_bobVars.lastSpeed = speed;

        lowerAmt = cvars.lower_amt * (speed * 0.001f);

        bobOffset = Map(speed, 0, 320, 0, 1);

        g_bobVars.bobTime += (time - g_bobVars.lastBobTime) * bobOffset;
        g_bobVars.lastBobTime = time;

        bobCycle = (((1000.0f - 150.0f) / 3.5f) * 0.001f) * cvars.bob_cycle * 1.25f;

        if (bobCycle <= 0.0f)
        {
            g_bobVars.vertBob = 0.0f;
            g_bobVars.horBob = 0.0f;
            return;
        }

        cycle = BobPhase(g_bobVars.bobTime, bobCycle);

        if (cycle < cvars.bob_up)
            cycle = kPiF * cycle / cvars.bob_up;
        else
            cycle = kPiF + kPiF * (cycle - cvars.bob_up) / (1.0f - cvars.bob_up);

        bobScale = 0.00625f;

        if (!onground)
            bobScale = 0.00125f;

        g_bobVars.vertBob = speed * (bobScale * cvars.amt_vert);
        g_bobVars.vertBob = (g_bobVars.vertBob * 0.3f + g_bobVars.vertBob * 0.7f * sinf(cycle));
        g_bobVars.vertBob = ClampBobFinite(g_bobVars.vertBob - lowerAmt, -8.f, 4.f);

        cycle = g_bobVars.bobTime - (int)(g_bobVars.bobTime / bobCycle * 2) * bobCycle * 2;
        cycle /= bobCycle * 2;

        if (cycle < cvars.bob_up)
            cycle = kPiF * cycle / cvars.bob_up;
        else
            cycle = kPiF + kPiF * (cycle - cvars.bob_up) / (1.0f - cvars.bob_up);

        g_bobVars.horBob = speed * (bobScale * cvars.amt_lat);
        g_bobVars.horBob = g_bobVars.horBob * 0.3f + g_bobVars.horBob * 0.7f * sinf(cycle);
        g_bobVars.horBob = ClampBobFinite(g_bobVars.horBob, -7.f, 4.f);
    }

    float V_CalcBob(double& bobtime, const view_bob::BobParams& cvars, float frametime, float speed)
    {
        bobtime += frametime;

        if (cvars.bob_cycle <= 0.0f)
            return 0.0f;

        float cycle = BobPhase(bobtime, cvars.bob_cycle);

        if (cycle < cvars.bob_up)
            cycle = kPiF * cycle / cvars.bob_up;
        else
            cycle = kPiF + kPiF * (cycle - cvars.bob_up) / (1.0f - cvars.bob_up);

        float bob = speed * cvars.bob;
        bob = bob * 0.3f + bob * 0.7f * sinf(cycle);
        return ClampBobFinite(bob, -7.f, 4.f);
    }
}

namespace
{
    // A frame sequence a player could produce: uneven frame times, speed that jumps and
    // coasts, air time now and then. Fixed seed, so a failure names a reproducible step.
    class FrameSequence
    {
    private:
        uint32_t state_;

    public:
        explicit FrameSequence(uint32_t seed) : state_(seed)
        {
        }

        float Next01()
        {
            state_ = state_ * 1664525u + 1013904223u;
            return static_cast<float>(state_ >> 8) / 16777216.0f;
        }

        float NextFrametime()
        {
            return 0.001f + Next01() * 0.05f;
        }

        float NextSpeed(float previous)
        {
            if (Next01() < 0.1f)
                return Next01() * 320.0f;

            return std::clamp(previous + (Next01() - 0.5f) * 40.0f, 0.0f, 320.0f);
        }

        bool NextOnground()
        {
            return Next01() > 0.15f;
        }
    };

    const view_bob::BobParams kParamSets[] = {
        {view_bob::kStyleClassic, 0.01f, 0.8f, 0.5f, 0.13f, 0.32f, 8.0f, true},
        {view_bob::kStyleModern, 0.05f, 0.1f, 0.05f, 0.4f, 0.8f, 30.0f, false},
        {view_bob::kStyleModern, 0.0f, 2.0f, 0.95f, 0.0f, 0.0f, 0.0f, false},
        {view_bob::kStyleClassic, 0.02f, 1.3f, 0.33f, 0.2f, 0.5f, 3.0f, true},
    };

    constexpr int kSteps = 4000;
}

TEST(ViewBobReference, ClassicBobMatchesTheOldClientBitForBit)
{
    for (const view_bob::BobParams& params : kParamSets)
    {
        FrameSequence frames(0x5EEDu);
        view_bob::ClassicBobState state{};
        double reference_time = 0.0;
        float speed = 0.0f;

        for (int step = 0; step < kSteps; step++)
        {
            float frametime = frames.NextFrametime();
            speed = frames.NextSpeed(speed);

            float expected = reference::V_CalcBob(reference_time, params, frametime, speed);
            float actual = view_bob::StepClassicBob(state, params, frametime, speed);

            ASSERT_EQ(actual, expected) << "step " << step << " cycle " << params.bob_cycle;
        }
    }
}

TEST(ViewBobReference, ModernBobMatchesTheOldClientBitForBit)
{
    for (const view_bob::BobParams& params : kParamSets)
    {
        FrameSequence frames(0xB0Bu);
        view_bob::ModernBobState state{};
        reference::BobVars reference_state{};
        float time = 0.0f;
        float speed = 0.0f;

        for (int step = 0; step < kSteps; step++)
        {
            time += frames.NextFrametime();
            speed = frames.NextSpeed(speed);
            bool onground = frames.NextOnground();

            reference::V_CalcBob_CSGO(reference_state, params, time, speed, onground);
            view_bob::ModernBobOffsets actual = view_bob::StepModernBob(state, params, time, speed, onground);

            ASSERT_EQ(actual.vert, reference_state.vertBob) << "step " << step << " cycle " << params.bob_cycle;
            ASSERT_EQ(actual.hor, reference_state.horBob) << "step " << step << " cycle " << params.bob_cycle;
        }

        EXPECT_EQ(state.bob_time, reference_state.bobTime);
        EXPECT_EQ(state.last_speed, reference_state.lastSpeed);
    }
}
