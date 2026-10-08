#include <gtest/gtest.h>

#include "RadarBroadcast.h"

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
    EXPECT_EQ(hostages[0].previous_pos, (ncl_math::Vector2{200.0f, 100.0f}));
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
