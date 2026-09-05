#include <cmath>

#include <gtest/gtest.h>

#include <view/view_bob.h>

namespace
{
    view_bob::BobParams ClassicParams()
    {
        view_bob::BobParams params{};
        params.style = view_bob::kStyleClassic;
        params.bob = 0.01f;
        params.bob_cycle = 0.8f;
        params.bob_up = 0.5f;

        return params;
    }

    view_bob::BobParams ModernParams()
    {
        view_bob::BobParams params{};
        params.style = view_bob::kStyleModern;
        params.bob_cycle = 0.8f;
        params.bob_up = 0.5f;
        params.amt_vert = 0.13f;
        params.amt_lat = 0.32f;
        params.lower_amt = 8.0f;

        return params;
    }
}

TEST(StepClassicBob, DisabledCycleAdvancesTimeAndReturnsZero)
{
    view_bob::ClassicBobState state{};
    view_bob::BobParams params = ClassicParams();
    params.bob_cycle = 0.0f;

    EXPECT_EQ(view_bob::StepClassicBob(state, params, 0.25f, 250.0f), 0.0f);
    EXPECT_DOUBLE_EQ(state.bob_time, 0.25);
}

TEST(StepClassicBob, PeaksAtTheTopOfTheRise)
{
    view_bob::ClassicBobState state{};
    view_bob::BobParams params = ClassicParams();

    // a quarter of the cycle in, halfway up the rise, the sine is at its top and the
    // speed times cl_bob comes through whole
    EXPECT_FLOAT_EQ(view_bob::StepClassicBob(state, params, 0.2f, 250.0f), 2.5f);
}

TEST(StepClassicBob, ClampsToTheEngineRange)
{
    view_bob::ClassicBobState state{};
    view_bob::BobParams params = ClassicParams();
    params.bob = 0.05f;

    EXPECT_FLOAT_EQ(view_bob::StepClassicBob(state, params, 0.2f, 320.0f), 4.0f);

    params.bob = -0.05f;
    EXPECT_FLOAT_EQ(view_bob::StepClassicBob(state, params, 0.8f, 320.0f), -7.0f);
}

TEST(StepClassicBob, NonFiniteInputComesOutAsZero)
{
    view_bob::ClassicBobState state{};
    view_bob::BobParams params = ClassicParams();
    params.bob = NAN;

    EXPECT_EQ(view_bob::StepClassicBob(state, params, 0.2f, 250.0f), 0.0f);
}

TEST(StepModernBob, DisabledCycleStillTracksTimeAndSpeed)
{
    view_bob::ModernBobState state{};
    view_bob::BobParams params = ModernParams();
    params.bob_cycle = 0.0f;

    view_bob::ModernBobOffsets offsets = view_bob::StepModernBob(state, params, 0.1f, 250.0f, true);

    EXPECT_EQ(offsets.vert, 0.0f);
    EXPECT_EQ(offsets.hor, 0.0f);
    EXPECT_FLOAT_EQ(state.last_bob_time, 0.1f);
    EXPECT_FLOAT_EQ(state.last_speed, 62.0f);
}

TEST(StepModernBob, SpeedFollowsAtTheAccelerationLimit)
{
    view_bob::ModernBobState state{};
    view_bob::BobParams params = ModernParams();

    view_bob::StepModernBob(state, params, 0.1f, 250.0f, true);
    EXPECT_FLOAT_EQ(state.last_speed, 62.0f);

    view_bob::StepModernBob(state, params, 0.2f, 250.0f, true);
    EXPECT_FLOAT_EQ(state.last_speed, 124.0f);

    view_bob::StepModernBob(state, params, 10.0f, 250.0f, true);
    EXPECT_FLOAT_EQ(state.last_speed, 250.0f);
}

TEST(StepModernBob, OffsetsStayInsideTheEngineRange)
{
    view_bob::ModernBobState state{};
    view_bob::BobParams params = ModernParams();
    params.amt_vert = 1000.0f;
    params.amt_lat = 1000.0f;
    params.lower_amt = 0.0f;

    for (int i = 1; i <= 200; i++)
    {
        view_bob::ModernBobOffsets offsets = view_bob::StepModernBob(state, params, i * 0.05f, 320.0f, true);

        EXPECT_GE(offsets.vert, -8.0f);
        EXPECT_LE(offsets.vert, 4.0f);
        EXPECT_GE(offsets.hor, -7.0f);
        EXPECT_LE(offsets.hor, 4.0f);
    }
}

TEST(StepModernBob, IsDeterministicForTheSameInputs)
{
    view_bob::ModernBobState first{};
    view_bob::ModernBobState second{};
    view_bob::BobParams params = ModernParams();

    for (int i = 1; i <= 50; i++)
    {
        view_bob::ModernBobOffsets a = view_bob::StepModernBob(first, params, i * 0.016f, 200.0f, i % 7 != 0);
        view_bob::ModernBobOffsets b = view_bob::StepModernBob(second, params, i * 0.016f, 200.0f, i % 7 != 0);

        EXPECT_EQ(a.vert, b.vert);
        EXPECT_EQ(a.hor, b.hor);
    }
}

TEST(PlaceClassicBob, MovesForwardOnlyWithoutSway)
{
    view_bob::BobOffsets offsets = view_bob::PlaceClassicBob(2.0f, view_bob::kStyleClassic);

    EXPECT_FLOAT_EQ(offsets.forward, 0.8f);
    EXPECT_EQ(offsets.up, 0.0f);
    EXPECT_EQ(offsets.side, 0.0f);
    EXPECT_EQ(offsets.pitch, 0.0f);
    EXPECT_EQ(offsets.yaw, 0.0f);
    EXPECT_EQ(offsets.roll, 0.0f);
}

TEST(PlaceClassicBob, SwayTurnsTheModelAgainstTheBob)
{
    view_bob::BobOffsets offsets = view_bob::PlaceClassicBob(2.0f, view_bob::kStyleClassicSway);

    EXPECT_FLOAT_EQ(offsets.forward, 0.8f);
    EXPECT_FLOAT_EQ(offsets.pitch, -0.6f);
    EXPECT_FLOAT_EQ(offsets.yaw, -1.0f);
    EXPECT_FLOAT_EQ(offsets.roll, -2.0f);
}

TEST(PlaceModernBob, SplitsTheVerticalBobBetweenForwardAndUp)
{
    view_bob::BobOffsets offsets = view_bob::PlaceModernBob(view_bob::ModernBobOffsets{2.0f, 3.0f});

    EXPECT_FLOAT_EQ(offsets.forward, 0.8f);
    EXPECT_FLOAT_EQ(offsets.up, 0.2f);
    EXPECT_FLOAT_EQ(offsets.side, 0.6f);
    EXPECT_EQ(offsets.pitch, 0.0f);
    EXPECT_EQ(offsets.yaw, 0.0f);
    EXPECT_EQ(offsets.roll, 0.0f);
}
