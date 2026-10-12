#include <gtest/gtest.h>

#include <utility>

#include "hud/radar/layers/players/DeathMarks.h"
#include "hud/radar/layers/players/PlayerLastSeen.h"

TEST(PlayerLastSeen, LosingSightRemembersTheLastVisiblePosition)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);

    EXPECT_TRUE(memory.get_marks().empty());

    seen[0].pos = {150.0f, 100.0f};
    memory.Update(seen, 2.0f);
    memory.Update({}, 3.0f);

    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].pos, (ncl_math::Vector3{150.0f, 100.0f}));
    EXPECT_EQ(memory.get_marks()[0].alpha, 255);
}

TEST(PlayerLastSeen, HoldsFadesAndExpiresAtTheSameTimeAsADeathMark)
{
    PlayerLastSeen memory;
    DeathMarks deaths;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 10.0f);
    deaths.Add(seen[0].pos, {255, 0, 0, 255}, 10.0f);

    const std::pair<float, int> timeline[] = {{10.0f, 255}, {12.0f, 255}, {13.0f, 255}, {13.75f, 127}, {14.0f, 85}, {14.49f, 1}};

    for (const std::pair<float, int>& sample : timeline)
    {
        SCOPED_TRACE(sample.first);
        int tolerance = sample.first == 14.0f ? 1 : 0;

        memory.Update({}, sample.first);
        DeathMarks::Visible visible[DeathMarks::kMax];
        ASSERT_EQ(deaths.CollectVisible(visible, sample.first), 1);
        ASSERT_EQ(memory.get_marks().size(), 1u);
        EXPECT_NEAR(memory.get_marks()[0].alpha, sample.second, tolerance);
        EXPECT_NEAR(visible[0].alpha, sample.second, tolerance);
        EXPECT_EQ(memory.get_marks()[0].alpha, visible[0].alpha);
    }

    memory.Update({}, 14.5f);
    DeathMarks::Visible visible[DeathMarks::kMax];
    EXPECT_EQ(deaths.CollectVisible(visible, 14.5f), 0);
    EXPECT_TRUE(memory.get_marks().empty());
    memory.Update({}, 15.0f);
    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, RepeatedMissingUpdatesDoNotExtendExpiration)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);

    for (float time : {2.1f, 3.0f, 4.0f, 5.0f, 6.49f})
    {
        memory.Update({}, time);
        ASSERT_EQ(memory.get_marks().size(), 1u);
        EXPECT_FLOAT_EQ(memory.get_marks()[0].time, 2.0f);
    }

    memory.Update({}, 6.5f);
    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, TimeBeforeCreationDiscardsTheMark)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 10.0f);
    memory.Update({}, 11.0f);
    memory.Update({}, 1.0f);

    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, AnotherPlayerAtTheSamePositionDoesNotClearTheMark)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    seen[0].player = 2;
    memory.Update(seen, 3.0f);

    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].player, 1);
}

TEST(PlayerLastSeen, ReturningPlayerReplacesTheOldPositionRegardlessOfDistance)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    seen[0].pos = {1000.0f, 100.0f};
    memory.Update(seen, 3.0f);

    EXPECT_TRUE(memory.get_marks().empty());

    memory.Update({}, 4.0f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].pos, (ncl_math::Vector3{1000.0f, 100.0f}));
    EXPECT_FLOAT_EQ(memory.get_marks()[0].time, 4.0f);
}

TEST(PlayerLastSeen, NearbyVisiblePlayerDoesNotPreventANewLoss)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {
        {1, {100.0f, 100.0f}, RadarTeam::Terrorist},
        {2, {200.0f, 100.0f}, RadarTeam::Terrorist},
    };
    memory.Update(seen, 1.0f);
    memory.Update({seen + 1, 1}, 1.5f);

    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].player, 1);

    memory.Update({}, 2.0f);
    ASSERT_EQ(memory.get_marks().size(), 2u);
}

TEST(PlayerLastSeen, DeathDoesNotCreateALostSightMark)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.OnPlayerDeath(1);
    memory.Update({}, 2.0f);

    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, ResetClearsBothMarksAndPendingSightings)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    seen[0].player = 2;
    seen[0].pos = {1000.0f, 100.0f};
    memory.Update(seen, 3.0f);

    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].player, 1);
    EXPECT_EQ(memory.get_marks()[0].pos, (ncl_math::Vector3{100.0f, 100.0f}));

    memory.Reset();
    EXPECT_TRUE(memory.get_marks().empty());

    memory.Update({}, 4.0f);
    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, DeathClearsOnlyTheDeadPlayersMarks)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {
        {1, {100.0f, 100.0f}, RadarTeam::Terrorist},
        {2, {1000.0f, 100.0f}, RadarTeam::Terrorist},
    };
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);

    ASSERT_EQ(memory.get_marks().size(), 2u);

    memory.OnPlayerDeath(1);

    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].pos, seen[1].pos);

    memory.Update({}, 3.0f);

    ASSERT_EQ(memory.get_marks().size(), 1u);

    memory.OnPlayerDeath(2);

    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, DeathClearsTheLatestLossOfThePlayer)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    seen[0].pos = {1000.0f, 100.0f};
    memory.Update(seen, 3.0f);
    memory.Update({}, 4.0f);

    ASSERT_EQ(memory.get_marks().size(), 1u);

    memory.OnPlayerDeath(1);

    EXPECT_TRUE(memory.get_marks().empty());

    memory.Update({}, 4.5f);

    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, KeepsObservedHeight)
{
    PlayerLastSeen last_seen;
    PlayerLastSeen::Observation seen[] = {{1, {64, 64}, RadarTeam::Terrorist}};
    seen[0].pos = ncl_math::Vector3{64, 64, 256};
    last_seen.Update(seen, 10.0f);
    last_seen.Update({}, 11.0f);

    ASSERT_EQ(last_seen.get_marks().size(), 1u);
    EXPECT_FLOAT_EQ(last_seen.get_marks()[0].pos.z, 256.0f);
}

TEST(PlayerLastSeen, OtherFloorObservationKeepsHistory)
{
    PlayerLastSeen last_seen;
    PlayerLastSeen::Observation seen[] = {{1, {64, 64}, RadarTeam::Terrorist}};
    last_seen.Update(seen, 10.0f);
    last_seen.Update({}, 11.0f);
    seen[0].player = 2;
    seen[0].pos = ncl_math::Vector3{64, 64, 512};
    last_seen.Update(seen, 12.0f);

    ASSERT_EQ(last_seen.get_marks().size(), 1u);
}

TEST(PlayerLastSeen, ForgetClearsTheMarkAndPendingObservation)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::CounterTerrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    ASSERT_EQ(memory.get_marks().size(), 1u);

    memory.Forget(1);
    EXPECT_TRUE(memory.get_marks().empty());

    memory.Update(seen, 3.0f);
    memory.Forget(1);
    memory.Update({}, 4.0f);
    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, LossFreezesTheMarkerFor150MillisecondsBeforeBecomingUncertain)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100, 100, 36}, RadarTeam::Terrorist, 90, true, true}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);

    for (float time : {2.0f, 2.1f, 2.149f})
    {
        memory.Update({}, time);
        ASSERT_EQ(memory.get_marks().size(), 1u);
        EXPECT_FALSE(memory.get_marks()[0].uncertain);
        EXPECT_EQ(memory.get_marks()[0].pos, (ncl_math::Vector3{100, 100, 36}));
        EXPECT_FLOAT_EQ(memory.get_marks()[0].yaw, 90.0f);
        EXPECT_TRUE(memory.get_marks()[0].has_dir);
        EXPECT_TRUE(memory.get_marks()[0].carrier);
        EXPECT_EQ(memory.get_marks()[0].alpha, 255);
    }

    memory.Update({}, 2.15f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_TRUE(memory.get_marks()[0].uncertain);
    EXPECT_EQ(memory.get_marks()[0].alpha, 255);
}

TEST(PlayerLastSeen, BriefLossDoesNotLeaveAnUncertainMarker)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100, 100, 36}, RadarTeam::CounterTerrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    memory.Update(seen, 2.1f);
    EXPECT_TRUE(memory.get_marks().empty());

    memory.Update({}, 2.2f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_FALSE(memory.get_marks()[0].uncertain);
    memory.Update({}, 2.351f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_TRUE(memory.get_marks()[0].uncertain);
}
