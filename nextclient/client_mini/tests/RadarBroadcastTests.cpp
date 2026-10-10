#include <gtest/gtest.h>

#include "hud/radar/markers/RadarBroadcast.h"
#include "hud/radar/policy/RadarPolicy.h"

TEST(RadarBroadcastPlayer, RepeatedCoordinatesStillAdvanceTheMessageSequence)
{
    RadarBroadcast broadcast;
    broadcast.SetPlayer(1, {0.0f, 0.0f, 50.0f}, 1.0f);
    std::uint32_t first = broadcast.get_player(1).seq;
    broadcast.SetPlayer(1, {0.0f, 0.0f, 50.0f}, 1.0f);

    EXPECT_GT(broadcast.get_player(1).seq, first);
    EXPECT_EQ(broadcast.get_player(1).pos, (ncl_math::Vector3{0.0f, 0.0f, 50.0f}));
}

TEST(RadarBroadcastPlayer, AZeroedMessageClearsThePositionAndAdvancesTheSequence)
{
    RadarBroadcast broadcast;
    broadcast.SetPlayer(1, {100.0f, 100.0f, 50.0f}, 1.0f);
    std::uint32_t first = broadcast.get_player(1).seq;
    broadcast.SetPlayer(1, {}, 1.0f);

    EXPECT_GT(broadcast.get_player(1).seq, first);
    EXPECT_EQ(broadcast.get_player(1).pos, ncl_math::Vector3{});
}

TEST(RadarBroadcastPlayer, RoundResetDropsThePreviousLifeWithoutReusingItsSequence)
{
    RadarBroadcast broadcast;
    broadcast.SetPlayer(1, {100.0f, 100.0f, 50.0f}, 1.0f);
    std::uint32_t first = broadcast.get_player(1).seq;
    broadcast.ResetPlayers();

    EXPECT_EQ(broadcast.get_player(1).seq, 0u);

    broadcast.SetPlayer(1, {200.0f, 100.0f, 50.0f}, 1.0f);

    EXPECT_GT(broadcast.get_player(1).seq, first);

    broadcast.Reset(10.0f);

    EXPECT_EQ(broadcast.get_player(1).seq, 0u);
}

TEST(RadarBroadcastPlayer, KeepsLastTwoReceivedPositionsWithoutFrameUpdates)
{
    RadarBroadcast broadcast;
    broadcast.SetPlayer(1, {100.0f, 100.0f, 50.0f}, 10.0f);

    EXPECT_FLOAT_EQ(broadcast.get_player(1).interval, 0.0f);

    broadcast.SetPlayer(1, {200.0f, 100.0f, 50.0f}, 11.0f);
    broadcast.SetPlayer(1, {500.0f, 100.0f, 50.0f}, 13.0f);

    EXPECT_EQ(broadcast.get_player(1).previous_pos, (ncl_math::Vector3{200.0f, 100.0f, 50.0f}));
    EXPECT_FLOAT_EQ(broadcast.get_player(1).time, 13.0f);
    EXPECT_FLOAT_EQ(broadcast.get_player(1).interval, 2.0f);
}

TEST(RadarBroadcastPlayer, RoundResetAndHiddenPositionsInvalidateTheMotionPair)
{
    RadarBroadcast broadcast;
    broadcast.SetPlayer(1, {100.0f, 100.0f, 50.0f}, 10.0f);
    broadcast.SetPlayer(1, {}, 11.0f);

    EXPECT_FLOAT_EQ(broadcast.get_player(1).interval, 0.0f);

    broadcast.SetPlayer(1, {200.0f, 100.0f, 50.0f}, 12.0f);

    EXPECT_FLOAT_EQ(broadcast.get_player(1).interval, 0.0f);

    broadcast.ResetPlayers();
    broadcast.SetPlayer(1, {300.0f, 100.0f, 50.0f}, 13.0f);

    EXPECT_FLOAT_EQ(broadcast.get_player(1).interval, 0.0f);
}

TEST(RadarBroadcastHostage, KeepsLastTwoReceivedPositionsAndClearsHistoryOnRemoval)
{
    RadarBroadcast broadcast;
    RadarBroadcast::Hostage hostages[kRadarMaxHostages];
    broadcast.SetHostage(3, {100.0f, 100.0f}, 10.0f);
    broadcast.SetHostage(3, {200.0f, 100.0f}, 11.0f);
    broadcast.SetHostage(3, {500.0f, 100.0f}, 13.0f);

    ASSERT_EQ(broadcast.CollectHostages(hostages), 1);
    EXPECT_EQ(hostages[0].previous_pos, (ncl_math::Vector3{200.0f, 100.0f}));
    EXPECT_FLOAT_EQ(hostages[0].interval, 2.0f);

    broadcast.KillHostage(3);
    broadcast.SetHostage(3, {600.0f, 100.0f}, 14.0f);

    ASSERT_EQ(broadcast.CollectHostages(hostages), 1);
    EXPECT_FLOAT_EQ(hostages[0].interval, 0.0f);

    broadcast.ResetHostages();
    broadcast.SetHostage(3, {700.0f, 100.0f}, 15.0f);

    ASSERT_EQ(broadcast.CollectHostages(hostages), 1);
    EXPECT_FLOAT_EQ(hostages[0].interval, 0.0f);
}

TEST(RadarBroadcast, HostageRetainsZ)
{
    RadarBroadcast broadcast;
    RadarBroadcast::Hostage out[1];
    broadcast.SetHostage(1, ncl_math::Vector3{64, 64, 256}, 10.0f);

    ASSERT_EQ(broadcast.CollectHostages(out), 1);
    EXPECT_FLOAT_EQ(out[0].pos.z, 256.0f);
}

TEST(RadarBroadcastHostage, ExpiredServerMarkersDoNotOccupyTheVisibleCollection)
{
    RadarBroadcast broadcast;
    RadarPolicy policy;
    policy.SetHostageHint(MessageHint::Marker);

    for (int index = 1; index <= kRadarMaxHostages; index++)
    {
        broadcast.SetHostage(index, {(float)index, 0.0f, 0.0f}, 0.0f);
    }

    broadcast.SetHostage(25, {25.0f, 0.0f, 0.0f}, 20.0f);

    RadarBroadcast::Hostage observations[RadarPolicy::kMaxHostages];
    int observation_count = broadcast.CollectHostages(observations);

    for (int i = 0; i < observation_count; i++)
    {
        policy.ObserveHostage({observations[i].index, observations[i].seq, observations[i].time}, 20.0f);
    }

    RadarBroadcast::Hostage markers[kRadarMaxHostages];
    int marker_count = broadcast.CollectHostageMarkers(markers, policy);

    ASSERT_EQ(marker_count, 1);
    EXPECT_EQ(markers[0].index, 25);
    EXPECT_TRUE(policy.IsHostageMarkerVisible(markers[0].index));
    EXPECT_EQ(broadcast.CollectHostages(observations), 25);
}

TEST(RadarBroadcastHostage, ServerMarkerCollectionFiltersTheFullByteRangeBeforeApplyingCapacity)
{
    RadarBroadcast broadcast;
    RadarPolicy policy;
    policy.SetHostageHint(MessageHint::Marker);

    for (int index = 0; index < RadarPolicy::kMaxHostages; index++)
    {
        float time = index < 231 ? 0.0f : 20.0f;
        broadcast.SetHostage(index, {(float)index, 0.0f, 0.0f}, time);
    }

    RadarBroadcast::Hostage observations[RadarPolicy::kMaxHostages];
    int observation_count = broadcast.CollectHostages(observations);
    ASSERT_EQ(observation_count, 256);

    for (int i = 0; i < observation_count; i++)
    {
        policy.ObserveHostage({observations[i].index, observations[i].seq, observations[i].time}, 20.0f);
    }

    RadarBroadcast::Hostage markers[kRadarMaxHostages];
    int marker_count = broadcast.CollectHostageMarkers(markers, policy);

    ASSERT_EQ(marker_count, 24);

    for (int i = 0; i < marker_count; i++)
    {
        EXPECT_EQ(markers[i].index, 231 + i);
        EXPECT_TRUE(policy.IsHostageMarkerVisible(markers[i].index));
    }

    EXPECT_EQ(broadcast.CollectHostages(observations), 256);
}

TEST(RadarBroadcastHostage, IndexAfterTheStockCapacityParticipatesInAutomaticClassification)
{
    RadarBroadcast broadcast;
    RadarPolicy policy;
    policy.SetMapObjectives(0, 24);

    for (int index = 1; index <= kRadarMaxHostages; index++)
    {
        broadcast.SetHostage(index, {(float)index, 0.0f, 0.0f}, 0.0f);
    }

    broadcast.SetHostage(255, {255.0f, 0.0f, 0.0f}, 20.0f);

    RadarBroadcast::Hostage observations[RadarPolicy::kMaxHostages];
    int observation_count = broadcast.CollectHostages(observations);

    for (int i = 0; i < observation_count; i++)
    {
        policy.ObserveHostage({observations[i].index, observations[i].seq, observations[i].time}, 20.0f);
    }

    RadarBroadcast::Hostage markers[kRadarMaxHostages];
    int marker_count = broadcast.CollectHostageMarkers(markers, policy);

    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    ASSERT_EQ(marker_count, 1);
    EXPECT_EQ(markers[0].index, 255);
}

TEST(RadarBroadcastHostage, StockPositionsRemainAvailableAfterTheMarkerTimeout)
{
    RadarBroadcast broadcast;
    RadarPolicy policy;
    policy.SetMapObjectives(0, 4);
    broadcast.SetHostage(1, {100.0f, 100.0f, 50.0f}, 0.0f);

    RadarBroadcast::Hostage hostages[kRadarMaxHostages];
    ASSERT_EQ(broadcast.CollectHostages(hostages), 1);
    policy.ObserveHostage({hostages[0].index, hostages[0].seq, hostages[0].time}, 20.0f);

    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Hostage);
    EXPECT_EQ(broadcast.CollectHostageMarkers(hostages, policy), 0);
    ASSERT_EQ(broadcast.CollectHostages(hostages), 1);
    EXPECT_EQ(hostages[0].index, 1);
    EXPECT_EQ(hostages[0].pos, (ncl_math::Vector3{100.0f, 100.0f, 50.0f}));
}
