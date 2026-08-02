#include <gtest/gtest.h>

#include "hud/radar/layers/players/PlayerTracks.h"

namespace
{
    // Samples are throttled at 0.10s; step well past that so each Sample records.
    constexpr float kStep = 0.2f;
} // namespace

TEST(PlayerTracksCopyPoints, PartialFillReturnsOldestToNewest)
{
    PlayerTracks tracks;
    ncl_math::Vector2 pts[3] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {20.0f, 0.0f}};

    for (int i = 0; i < 3; i++)
    {
        tracks.Sample(1, pts[i], 1.0f + kStep * i);
    }

    ncl_math::Vector2 out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, 3);
    EXPECT_EQ(out[0], pts[0]);
    EXPECT_EQ(out[1], pts[1]);
    EXPECT_EQ(out[2], pts[2]);
}

TEST(PlayerTracksCopyPoints, FullBufferWrapsAndKeepsNewest)
{
    PlayerTracks tracks;
    const int total = PlayerTracks::kMaxPoints + 6; // force a wrap past the ring

    for (int i = 0; i < total; i++)
    {
        tracks.Sample(1, {5.0f * i, 0.0f}, 1.0f + kStep * i);
    }

    ncl_math::Vector2 out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, PlayerTracks::kMaxPoints);
    // Oldest retained sample is index (total - kMaxPoints); newest is total-1.
    EXPECT_EQ(out[0], (ncl_math::Vector2{5.0f * (total - PlayerTracks::kMaxPoints), 0.0f}));
    EXPECT_EQ(out[n - 1], (ncl_math::Vector2{5.0f * (total - 1), 0.0f}));

    for (int i = 1; i < n; i++)
    {
        EXPECT_GT(out[i].x, out[i - 1].x); // strictly oldest-to-newest, no rotation
    }
}

TEST(PlayerTracksSample, JumpBreaksTheTrail)
{
    PlayerTracks tracks;
    tracks.Sample(1, {0.0f, 0.0f}, 1.0f);
    tracks.Sample(1, {10.0f, 0.0f}, 1.0f + kStep);
    tracks.Sample(1, {1000.0f, 0.0f}, 1.0f + kStep * 2); // > kJumpDist from the last point

    ncl_math::Vector2 out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, 1);
    EXPECT_EQ(out[0], (ncl_math::Vector2{1000.0f, 0.0f}));
}

TEST(PlayerTracksSample, ThrottlesWithinInterval)
{
    PlayerTracks tracks;
    tracks.Sample(1, {0.0f, 0.0f}, 1.0f);
    tracks.Sample(1, {10.0f, 0.0f}, 1.05f); // 0.05s < 0.10s interval: dropped

    ncl_math::Vector2 out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, 1);
    EXPECT_EQ(out[0], (ncl_math::Vector2{0.0f, 0.0f}));
}
