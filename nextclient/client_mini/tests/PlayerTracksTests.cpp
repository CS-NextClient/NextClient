#include <gtest/gtest.h>

#include <cmath>

#include "hud/radar/layers/players/PlayerTracks.h"
#include "hud/radar/markers/RadarLayerMarker.h"

namespace
{
    // Samples are throttled at 0.10s; step well past that so each Sample records.
    constexpr float kStep = 0.2f;
} // namespace

TEST(PlayerTracksCopyPoints, PartialFillReturnsOldestToNewest)
{
    PlayerTracks tracks;
    ncl_math::Vector3 pts[3] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {20.0f, 0.0f}};

    for (int i = 0; i < 3; i++)
    {
        tracks.Sample(1, pts[i], -1, 1.0f + kStep * i);
    }

    PlayerTrackPoint out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, 3);
    EXPECT_EQ(out[0].pos, pts[0]);
    EXPECT_EQ(out[1].pos, pts[1]);
    EXPECT_EQ(out[2].pos, pts[2]);
}

TEST(PlayerTracksCopyPoints, FullBufferWrapsAndKeepsNewest)
{
    PlayerTracks tracks;
    const int total = PlayerTracks::kMaxPoints + 6; // force a wrap past the ring

    for (int i = 0; i < total; i++)
    {
        tracks.Sample(1, {5.0f * i, 0.0f}, -1, 1.0f + kStep * i);
    }

    PlayerTrackPoint out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, PlayerTracks::kMaxPoints);
    // Oldest retained sample is index (total - kMaxPoints); newest is total-1.
    EXPECT_EQ(out[0].pos, (ncl_math::Vector3{5.0f * (total - PlayerTracks::kMaxPoints), 0.0f}));
    EXPECT_EQ(out[n - 1].pos, (ncl_math::Vector3{5.0f * (total - 1), 0.0f}));

    for (int i = 1; i < n; i++)
    {
        EXPECT_GT(out[i].pos.x, out[i - 1].pos.x); // strictly oldest-to-newest, no rotation
    }
}

TEST(PlayerTracksSample, JumpBreaksTheTrail)
{
    PlayerTracks tracks;
    tracks.Sample(1, {0.0f, 0.0f}, -1, 1.0f);
    tracks.Sample(1, {10.0f, 0.0f}, -1, 1.0f + kStep);
    tracks.Sample(1, {1000.0f, 0.0f}, -1, 1.0f + kStep * 2); // > kJumpDist from the last point

    PlayerTrackPoint out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, 1);
    EXPECT_EQ(out[0].pos, (ncl_math::Vector3{1000.0f, 0.0f}));
}

TEST(PlayerTracksSample, SparseBroadcastPositionsKeepTheTrail)
{
    PlayerTracks tracks;
    tracks.Sample(1, {100.0f, 0.0f}, -1, 1.0f);
    tracks.Sample(1, {120.0f, 0.0f}, -1, 1.2f);
    tracks.Sample(1, {600.0f, 0.0f}, -1, 3.0f, false);

    PlayerTrackPoint out[PlayerTracks::kMaxPoints];
    ASSERT_EQ(tracks.CopyPoints(1, out), 3);
    EXPECT_EQ(out[0].pos, (ncl_math::Vector3{100.0f, 0.0f}));
    EXPECT_EQ(out[2].pos, (ncl_math::Vector3{600.0f, 0.0f}));
}

TEST(PlayerTracksSample, ThrottlesWithinInterval)
{
    PlayerTracks tracks;
    tracks.Sample(1, {0.0f, 0.0f}, -1, 1.0f);
    tracks.Sample(1, {10.0f, 0.0f}, -1, 1.05f); // 0.05s < 0.10s interval: dropped

    PlayerTrackPoint out[PlayerTracks::kMaxPoints];
    int n = tracks.CopyPoints(1, out);

    ASSERT_EQ(n, 1);
    EXPECT_EQ(out[0].pos, (ncl_math::Vector3{0.0f, 0.0f}));
}

TEST(PlayerTracksSample, AStationaryBroadcastDoesNotEraseTheTrail)
{
    PlayerTracks tracks;
    tracks.Sample(1, {100.0f, 100.0f}, -1, 1.0f);
    tracks.Sample(1, {120.0f, 100.0f}, -1, 1.2f);

    for (int i = 0; i < 100; i++)
    {
        tracks.Sample(1, {120.0f, 100.0f}, -1, 1.4f + kStep * i);
    }

    PlayerTrackPoint out[PlayerTracks::kMaxPoints];
    int count = tracks.CopyPoints(1, out);
    ASSERT_EQ(count, 2);
    EXPECT_EQ(out[0].pos, (ncl_math::Vector3{100.0f, 100.0f}));
    EXPECT_EQ(out[1].pos, (ncl_math::Vector3{120.0f, 100.0f}));
}

TEST(PlayerTracks, TrailDoesNotBridgeOtherFloor)
{
    PlayerTracks tracks;
    tracks.Sample(1, {0, 0, 1}, 0, 10);
    tracks.Sample(1, {10, 0, 1}, 0, 10.2f);
    tracks.Sample(1, {10, 0, 257}, 1, 10.4f);
    tracks.Sample(1, {20, 0, 1}, 0, 10.6f);
    tracks.Sample(1, {30, 0, 1}, 0, 10.8f);
    PlayerTrackPoint points[PlayerTracks::kMaxPoints];

    ASSERT_EQ(tracks.CopyPoints(1, points), 5);

    RadarLayerView view{nullptr, 0, {}, true};
    EXPECT_TRUE(RadarLayerMarker_IsTrailSegmentVisible(view, points[0].layer, points[1].layer));
    EXPECT_FALSE(RadarLayerMarker_IsTrailSegmentVisible(view, points[1].layer, points[2].layer));
    EXPECT_FALSE(RadarLayerMarker_IsTrailSegmentVisible(view, points[2].layer, points[3].layer));
    EXPECT_TRUE(RadarLayerMarker_IsTrailSegmentVisible(view, points[3].layer, points[4].layer));
    EXPECT_FLOAT_EQ(points[2].pos.z, 257);
}

TEST(PlayerTracksSampleDue, PreservesIntervalBoundaryAndBackwardsTime)
{
    PlayerTracks tracks;
    EXPECT_FALSE(tracks.is_sample_due(1, std::nextafter(0.1f, 0.0f)));
    EXPECT_TRUE(tracks.is_sample_due(1, 0.1f));
    tracks.Sample(1, {1.0f, 0.0f, 0.0f}, 0, 0.1f);
    EXPECT_FALSE(tracks.is_sample_due(1, 0.1f));
    EXPECT_FALSE(tracks.is_sample_due(1, std::nextafter(0.2f, 0.0f)));
    EXPECT_TRUE(tracks.is_sample_due(1, 0.2f));
    EXPECT_TRUE(tracks.is_sample_due(1, std::nextafter(0.2f, 1.0f)));
    EXPECT_TRUE(tracks.is_sample_due(1, 0.05f));
    tracks.Sample(1, {2.0f, 0.0f, 0.0f}, 0, 0.05f);
    EXPECT_FALSE(tracks.is_sample_due(1, 0.05f));
    tracks.Clear(1);
    EXPECT_TRUE(tracks.is_sample_due(1, 0.1f));
    tracks.Sample(1, {3.0f, 0.0f, 0.0f}, 0, 1.0f);
    tracks.Reset();
    EXPECT_TRUE(tracks.is_sample_due(1, 0.1f));
}

TEST(PlayerTracksSampleDue, StationarySamplesAdvanceTheInterval)
{
    PlayerTracks tracks;
    tracks.Sample(1, {1.0f, 0.0f, 0.0f}, 0, 1.0f);
    tracks.Sample(1, {1.0f, 0.0f, 0.0f}, 0, 2.0f);
    EXPECT_FALSE(tracks.is_sample_due(1, 2.05f));
    tracks.Sample(1, {2.0f, 0.0f, 0.0f}, 0, 2.05f);
    PlayerTrackPoint points[2];
    ASSERT_EQ(tracks.CopyPoints(1, points), 1);
    EXPECT_EQ(points[0].pos, (ncl_math::Vector3{1.0f, 0.0f, 0.0f}));
    EXPECT_TRUE(tracks.is_sample_due(1, 2.2f));
}

TEST(PlayerTracksSampleDue, SuppressionEndsAtDeadlineAndBackwardsTimeExitsWindow)
{
    PlayerTracks tracks;
    tracks.Reset();
    tracks.BeginFrame(10.0f);
    EXPECT_TRUE(tracks.IsSuppressed(10.0f));
    EXPECT_TRUE(tracks.IsSuppressed(std::nextafter(10.5f, 10.0f)));
    EXPECT_FALSE(tracks.IsSuppressed(10.5f));
    EXPECT_FALSE(tracks.IsSuppressed(9.0f));
    EXPECT_TRUE(tracks.is_sample_due(1, 10.5f));
    tracks.Sample(1, {10.0f, 0.0f, 0.0f}, 0, 10.5f);
    tracks.Clear(1);
    EXPECT_TRUE(tracks.is_sample_due(1, 10.51f));
}
