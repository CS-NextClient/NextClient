#include <array>

#include <gtest/gtest.h>

#include "hud/radar/layers/players/PlayerMarkerFade.h"

TEST(PlayerMarkerFade, BroadcastPositionChangesImmediatelyWithAFixedPositionCrossfade)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 20.0f, 30.0f}, 10.0f, false, 1);
    const ncl_math::Vector3 target{400.0f, 500.0f, 600.0f};
    PlayerMarkerFade::Visible first = fade.Update(target, 11.0f, false, 2);
    EXPECT_EQ(first.pos, target);
    EXPECT_FLOAT_EQ(first.alpha, 0.0f);
    EXPECT_EQ(first.previous_pos, (ncl_math::Vector3{100.0f, 20.0f, 30.0f}));
    EXPECT_FLOAT_EQ(first.previous_alpha, 1.0f);
    PlayerMarkerFade::Visible middle = fade.Update(target, 11.06f, false, 2);
    EXPECT_EQ(middle.pos, target);
    EXPECT_EQ(middle.previous_pos, first.previous_pos);
    EXPECT_NEAR(middle.alpha, 0.5f, 0.00001f);
    EXPECT_NEAR(middle.previous_alpha, 0.5f, 0.00001f);
    PlayerMarkerFade::Visible end = fade.Update(target, 11.13f, false, 2);
    EXPECT_EQ(end.pos, target);
    EXPECT_FLOAT_EQ(end.alpha, 1.0f);
    EXPECT_FLOAT_EQ(end.previous_alpha, 0.0f);
}

TEST(PlayerMarkerFade, PvsLossHoldsLastSnapshotUntilANewBroadcast)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1);
    EXPECT_FLOAT_EQ(fade.Update({90.0f, 0.0f, 0.0f}, 10.1f, false, 1).pos.x, 100.0f);
    EXPECT_FLOAT_EQ(fade.Update({300.0f, 0.0f, 0.0f}, 10.2f, false, 2).pos.x, 300.0f);
}

TEST(PlayerMarkerFade, SnapshotReturnDoesNotInterpolateThePosition)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, false, 1);
    PlayerMarkerFade::Visible first = fade.Update({300.0f, 0.0f, 0.0f}, 11.0f, true, 1);
    EXPECT_FLOAT_EQ(first.pos.x, 300.0f);
    EXPECT_FLOAT_EQ(first.previous_pos.x, 100.0f);
    EXPECT_FLOAT_EQ(first.previous_alpha, 1.0f);
    PlayerMarkerFade::Visible next = fade.Update({320.0f, 0.0f, 0.0f}, 11.06f, true, 1);
    EXPECT_FLOAT_EQ(next.pos.x, 320.0f);
    EXPECT_FLOAT_EQ(next.previous_pos.x, 100.0f);
    EXPECT_NEAR(next.alpha, 0.5f, 0.00001f);
    EXPECT_NEAR(next.previous_alpha, 0.5f, 0.00001f);
}

TEST(PlayerMarkerFade, ContinuousSnapshotsRemainOpaque)
{
    PlayerMarkerFade fade;
    EXPECT_FLOAT_EQ(fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 0).alpha, 1.0f);
    PlayerMarkerFade::Visible next = fade.Update({120.0f, 0.0f, 0.0f}, 10.02f, true, 0);
    EXPECT_FLOAT_EQ(next.pos.x, 120.0f);
    EXPECT_FLOAT_EQ(next.alpha, 1.0f);
    EXPECT_FLOAT_EQ(next.previous_alpha, 0.0f);
}

TEST(PlayerMarkerFade, StationaryMessagesDoNotRestartTheFade)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1);
    fade.Update({300.0f, 0.0f, 0.0f}, 11.0f, false, 2);
    PlayerMarkerFade::Visible middle = fade.Update({300.0f, 0.0f, 0.0f}, 11.06f, false, 3);
    EXPECT_NEAR(middle.alpha, 0.5f, 0.00001f);
    EXPECT_FLOAT_EQ(fade.Update({300.0f, 0.0f, 0.0f}, 11.13f, false, 4).alpha, 1.0f);
}

TEST(PlayerMarkerFade, FirstBroadcastFadesInWithoutAnOldMarker)
{
    PlayerMarkerFade fade;
    PlayerMarkerFade::Visible first = fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, false, 1);
    EXPECT_FLOAT_EQ(first.pos.x, 100.0f);
    EXPECT_FLOAT_EQ(first.alpha, 0.0f);
    EXPECT_FLOAT_EQ(first.previous_alpha, 0.0f);
    EXPECT_FLOAT_EQ(fade.Update(first.pos, 10.13f, false, 1).alpha, 1.0f);
}

TEST(PlayerMarkerFade, VisibilityLossDoesNotCarryAnOldGlyphOnReturn)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 0);
    fade.Lose();
    PlayerMarkerFade::Visible next = fade.Update({300.0f, 0.0f, 0.0f}, 10.5f, true, 0);
    EXPECT_FLOAT_EQ(next.pos.x, 300.0f);
    EXPECT_FLOAT_EQ(next.alpha, 0.0f);
    EXPECT_FLOAT_EQ(next.previous_alpha, 0.0f);
}

TEST(PlayerMarkerFade, ResetAndTimeRewindDiscardThePreviousPosition)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1);
    PlayerMarkerFade::Visible rewind = fade.Update({300.0f, 0.0f, 0.0f}, 9.0f, false, 1);
    EXPECT_FLOAT_EQ(rewind.pos.x, 300.0f);
    EXPECT_FLOAT_EQ(rewind.previous_alpha, 0.0f);
    fade.Reset();
    PlayerMarkerFade::Visible reset = fade.Update({400.0f, 0.0f, 0.0f}, 10.0f, true, 1);
    EXPECT_FLOAT_EQ(reset.pos.x, 400.0f);
    EXPECT_FLOAT_EQ(reset.alpha, 1.0f);
    EXPECT_FLOAT_EQ(reset.previous_alpha, 0.0f);
}

TEST(PlayerMarkerFade, RapidMessagesNeverInventAnIntermediatePosition)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1);
    fade.Update({300.0f, 0.0f, 0.0f}, 11.0f, false, 2);
    PlayerMarkerFade::Visible next = fade.Update({500.0f, 0.0f, 0.0f}, 11.03f, false, 3);
    EXPECT_FLOAT_EQ(next.pos.x, 500.0f);
    EXPECT_TRUE(next.previous_pos.x == 100.0f || next.previous_pos.x == 300.0f);
    EXPECT_GE(next.alpha, 0.0f);
    EXPECT_GE(next.previous_alpha, 0.0f);
    EXPECT_LE(next.alpha + next.previous_alpha, 1.00001f);
    EXPECT_FLOAT_EQ(fade.Update(next.pos, 11.16f, false, 3).alpha, 1.0f);
}

TEST(PlayerMarkerFade, FadeDurationIsIndependentOfFrameRate)
{
    for (int fps : {30, 60, 144})
    {
        SCOPED_TRACE(fps);
        PlayerMarkerFade fade;
        fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1);
        fade.Update({400.0f, 500.0f, 600.0f}, 11.0f, false, 2);
        for (int frame = 1; frame < fps / 5; frame++)
        {
            float elapsed = (float)frame / fps;
            PlayerMarkerFade::Visible shown = fade.Update({400.0f, 500.0f, 600.0f}, 11.0f + elapsed, false, 2);
            EXPECT_EQ(shown.pos, (ncl_math::Vector3{400.0f, 500.0f, 600.0f}));
            EXPECT_EQ(shown.previous_pos, (ncl_math::Vector3{100.0f, 0.0f, 0.0f}));
            if (elapsed >= PlayerMarkerFade::kDuration)
            {
                EXPECT_FLOAT_EQ(shown.alpha, 1.0f);
                EXPECT_FLOAT_EQ(shown.previous_alpha, 0.0f);
            }
        }
    }
}

TEST(PlayerMarkerFade, SmallBroadcastCorrectionsKeepOpaqueConfirmedPositions)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1, 10.0f);
    std::uint32_t seq = 2;
    for (float x : {108.0f, 103.0f, 113.0f, 105.0f})
    {
        PlayerMarkerFade::Visible marker = fade.Update({x, 0.0f, 0.0f}, 10.0f + seq * 0.09f, false, seq, 10.0f);
        EXPECT_FLOAT_EQ(marker.pos.x, x);
        EXPECT_FLOAT_EQ(marker.alpha, 1.0f);
        EXPECT_FLOAT_EQ(marker.previous_alpha, 0.0f);
        seq++;
    }
}

TEST(PlayerMarkerFade, SmallCorrectionDuringFadeDoesNotRestartIt)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1, 10.0f);
    fade.Update({300.0f, 0.0f, 0.0f}, 11.0f, false, 2, 10.0f);
    PlayerMarkerFade::Visible next = fade.Update({306.0f, 0.0f, 0.0f}, 11.09f, false, 3, 10.0f);
    EXPECT_FLOAT_EQ(next.pos.x, 306.0f);
    EXPECT_FLOAT_EQ(next.previous_pos.x, 100.0f);
    EXPECT_NEAR(next.alpha, 0.75f, 0.00001f);
    EXPECT_NEAR(next.previous_alpha, 0.25f, 0.00001f);
    EXPECT_FLOAT_EQ(fade.Update(next.pos, 11.13f, false, 3, 10.0f).alpha, 1.0f);
}

TEST(PlayerMarkerFade, RapidLargeReportsPreserveVisibleOpacity)
{
    for (float spacing : {0.02f, 0.08f, 0.1f})
    {
        SCOPED_TRACE(spacing);
        PlayerMarkerFade fade;
        fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, true, 1);
        fade.Update({300.0f, 0.0f, 0.0f}, 11.0f, false, 2);
        for (std::uint32_t seq = 3; seq < 7; seq++)
        {
            float x = 300.0f + (seq - 2) * 200.0f;
            PlayerMarkerFade::Visible next = fade.Update({x, 0.0f, 0.0f}, 11.0f + (seq - 2) * spacing, false, seq);
            EXPECT_FLOAT_EQ(next.pos.x, x);
            EXPECT_NEAR(next.alpha + next.previous_alpha, 1.0f, 0.00001f);
        }
    }
}

TEST(PlayerMarkerFade, SmallSnapshotReturnKeepsOpaqueConfirmedPosition)
{
    PlayerMarkerFade fade;
    fade.Update({100.0f, 0.0f, 0.0f}, 10.0f, false, 1, 10.0f);
    fade.Update({100.0f, 0.0f, 0.0f}, 10.2f, false, 1, 10.0f);
    PlayerMarkerFade::Visible next = fade.Update({106.0f, 0.0f, 0.0f}, 10.21f, true, 1, 10.0f);
    EXPECT_FLOAT_EQ(next.pos.x, 106.0f);
    EXPECT_FLOAT_EQ(next.alpha, 1.0f);
    EXPECT_FLOAT_EQ(next.previous_alpha, 0.0f);
}

TEST(PlayerMarkerFade, UnsequencedFallbackCannotReplaceTheLastSnapshot)
{
    PlayerMarkerFade fade;
    const ncl_math::Vector3 snapshot{-713.8f, -2153.3f, -284.0f};
    const ncl_math::Vector3 fallback{-929.3f, -2168.1f, -283.9f};
    fade.Update(snapshot, 10.0f, true, 0, 10.0f);
    for (float time : {10.01f, 10.09f, 10.2f})
    {
        PlayerMarkerFade::Visible shown = fade.Update(fallback, time, false, 0, 10.0f);
        EXPECT_EQ(shown.pos, snapshot);
        EXPECT_FLOAT_EQ(shown.alpha, 1.0f);
        EXPECT_FLOAT_EQ(shown.previous_alpha, 0.0f);
    }
}

TEST(PlayerMarkerFade, FirstUnsequencedPositionStaysAvailableUntilANewReport)
{
    PlayerMarkerFade fade;
    const ncl_math::Vector3 initial{100.0f, 20.0f, 30.0f};
    fade.Update(initial, 10.0f, false, 0, 10.0f);
    EXPECT_FLOAT_EQ(fade.Update(initial, 10.13f, false, 0, 10.0f).alpha, 1.0f);
    PlayerMarkerFade::Visible retained = fade.Update({300.0f, 20.0f, 30.0f}, 11.0f, false, 0, 10.0f);
    EXPECT_EQ(retained.pos, initial);
    EXPECT_FLOAT_EQ(retained.alpha, 1.0f);
    const ncl_math::Vector3 report{400.0f, 50.0f, 60.0f};
    PlayerMarkerFade::Visible updated = fade.Update(report, 11.1f, false, 1, 10.0f);
    EXPECT_EQ(updated.pos, report);
    EXPECT_EQ(updated.previous_pos, initial);
    EXPECT_FLOAT_EQ(updated.alpha, 0.0f);
    EXPECT_FLOAT_EQ(updated.previous_alpha, 1.0f);
    EXPECT_FLOAT_EQ(fade.Update(report, 11.23f, false, 1, 10.0f).alpha, 1.0f);
}

TEST(PlayerMarkerFade, ConcurrentPvsLossKeepsAllUnsequencedAlliesOpaque)
{
    std::array<PlayerMarkerFade, 32> markers;
    for (int player = 0; player < 32; player++)
    {
        markers[player].Update({1000.0f + player * 64.0f, 0.0f, 0.0f}, 10.0f, true, 0, 10.0f);
    }
    for (int player = 0; player < 32; player++)
    {
        SCOPED_TRACE(player);
        PlayerMarkerFade::Visible shown = markers[player].Update({800.0f + player * 64.0f, 0.0f, 0.0f}, 11.0f, false, 0, 10.0f);
        EXPECT_FLOAT_EQ(shown.pos.x, 1000.0f + player * 64.0f);
        EXPECT_FLOAT_EQ(shown.alpha, 1.0f);
        EXPECT_FLOAT_EQ(shown.previous_alpha, 0.0f);
    }
}
