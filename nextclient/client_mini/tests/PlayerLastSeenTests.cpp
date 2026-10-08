#include <gtest/gtest.h>

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
    EXPECT_EQ(memory.get_marks()[0].pos, (ncl_math::Vector2{150.0f, 100.0f}));
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

    for (float time : {10.0f, 12.0f, 13.0f, 13.75f, 14.0f, 14.49f})
    {
        SCOPED_TRACE(time);
        memory.Update({}, time);
        DeathMarks::Visible visible[DeathMarks::kMax];
        ASSERT_EQ(deaths.CollectVisible(visible, time), 1);
        ASSERT_EQ(memory.get_marks().size(), 1u);
        EXPECT_EQ(memory.get_marks()[0].alpha, visible[0].alpha);
    }

    memory.Update({}, 14.5f);
    DeathMarks::Visible visible[DeathMarks::kMax];
    EXPECT_EQ(deaths.CollectVisible(visible, 14.5f), 0);
    EXPECT_TRUE(memory.get_marks().empty());
    memory.Update({}, 15.0f);
    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, EachLossHasItsOwnExpirationTime)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    seen[0].pos = {1000.0f, 100.0f};
    memory.Update(seen, 3.0f);
    memory.Update({}, 4.0f);
    ASSERT_EQ(memory.get_marks().size(), 2u);

    memory.Update({}, 6.5f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].pos, seen[0].pos);
    EXPECT_EQ(memory.get_marks()[0].alpha, 255);
    memory.Update({}, 8.5f);
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

TEST(PlayerLastSeen, AnyVisibleEnemyClearsNearbyMarksAtTheRadiusBoundary)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    PlayerLastSeen::Observation other[] = {{2, {357.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(other, 3.0f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    other[0].pos.x = 356.0f;
    memory.Update(other, 3.0f);
    EXPECT_TRUE(memory.get_marks().empty());
}

TEST(PlayerLastSeen, ReappearingFarAwayKeepsTheOldMark)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    seen[0].pos = {1000.0f, 100.0f};
    memory.Update(seen, 3.0f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].pos, (ncl_math::Vector2{100.0f, 100.0f}));
    memory.Update({}, 4.0f);
    EXPECT_EQ(memory.get_marks().size(), 2u);
}

TEST(PlayerLastSeen, AnotherEnemyAlreadyInSightClearsANewLossInTheSameUpdate)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {
        {1, {100.0f, 100.0f}, RadarTeam::Terrorist},
        {2, {200.0f, 100.0f}, RadarTeam::Terrorist},
    };
    memory.Update(seen, 1.0f);
    memory.Update({seen + 1, 1}, 1.5f);
    EXPECT_TRUE(memory.get_marks().empty());
    memory.Update({}, 2.0f);
    ASSERT_EQ(memory.get_marks().size(), 1u);
    EXPECT_EQ(memory.get_marks()[0].pos, (ncl_math::Vector2{200.0f, 100.0f}));
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
    seen[0].pos = {1000.0f, 100.0f};
    memory.Update(seen, 3.0f);
    memory.Reset();
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

TEST(PlayerLastSeen, DeathClearsEveryRememberedPositionOfThePlayer)
{
    PlayerLastSeen memory;
    PlayerLastSeen::Observation seen[] = {{1, {100.0f, 100.0f}, RadarTeam::Terrorist}};
    memory.Update(seen, 1.0f);
    memory.Update({}, 2.0f);
    seen[0].pos = {1000.0f, 100.0f};
    memory.Update(seen, 3.0f);
    memory.Update({}, 4.0f);
    ASSERT_EQ(memory.get_marks().size(), 2u);

    memory.OnPlayerDeath(1);
    EXPECT_TRUE(memory.get_marks().empty());
    memory.Update({}, 4.5f);
    EXPECT_TRUE(memory.get_marks().empty());
}
