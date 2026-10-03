#include <gtest/gtest.h>

#include "ObjectiveSource.h"

namespace
{
    constexpr ObjectiveAudience kTerrorist{RadarTeam::Terrorist, false};
    constexpr ObjectiveAudience kCounterTerrorist{RadarTeam::CounterTerrorist, false};
    constexpr ObjectiveAudience kUnassigned{RadarTeam::Unassigned, false};
    constexpr ObjectiveAudience kSpectator{RadarTeam::Spectator, true};
} // namespace

TEST(ObjectiveSourceBomb, MarkerIntentOutranksTheAudience)
{
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Marker, kTerrorist), BombSource::ServerMarker);
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Marker, kCounterTerrorist), BombSource::ServerMarker);
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Marker, kSpectator), BombSource::ServerMarker);
}

TEST(ObjectiveSourceBomb, TerroristsReadTheBroadcast)
{
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Bomb, kTerrorist), BombSource::Broadcast);
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Unknown, kTerrorist), BombSource::Broadcast);
}

TEST(ObjectiveSourceBomb, SpectatorsReadTheBroadcastWhateverTeamTheyWatch)
{
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Bomb, kSpectator), BombSource::Broadcast);
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Bomb, {RadarTeam::CounterTerrorist, true}), BombSource::Broadcast);
}

TEST(ObjectiveSourceBomb, CounterTerroristsFallBackToSight)
{
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Bomb, kCounterTerrorist), BombSource::Sight);
}

TEST(ObjectiveSourceBomb, NoTeamAndNotSpectatingDrawsNothing)
{
    EXPECT_EQ(ObjectiveSource_ChooseBomb(BombIntent::Bomb, kUnassigned), BombSource::None);
}

TEST(ObjectiveSourceBombReport, ADropReportedWithinTheSecondIsCurrent)
{
    EXPECT_TRUE(ObjectiveSource_IsBombReportCurrent(false, 0.9f));
}

TEST(ObjectiveSourceBombReport, ADropNoLongerReportedIsStale)
{
    // A Terrorist who died hears neither the repeats nor the pickup.
    EXPECT_FALSE(ObjectiveSource_IsBombReportCurrent(false, 10.0f));
}

TEST(ObjectiveSourceBombReport, SpectatorsIgnoreDropsRetainedFromBeforeDeath)
{
    EXPECT_FALSE(ObjectiveSource_IsBombReportCurrent(false, 0.0f, true));
    EXPECT_FALSE(ObjectiveSource_IsBombReportCurrent(false, 0.9f, true));
    EXPECT_TRUE(ObjectiveSource_IsBombReportCurrent(false, 0.9f, false));
}

TEST(ObjectiveSourceBombReport, SpectatorsKeepPlantReports)
{
    EXPECT_TRUE(ObjectiveSource_IsBombReportCurrent(true, 60.0f, true));
}

TEST(ObjectiveSourceBombReport, DropsFromBeforeATimeResetAreStale)
{
    EXPECT_FALSE(ObjectiveSource_IsBombReportCurrent(false, -1.0f));
}

TEST(ObjectiveSourceBombReport, APlantHoldsUntilCleared)
{
    EXPECT_TRUE(ObjectiveSource_IsBombReportCurrent(true, 60.0f));
}

TEST(ObjectiveSourceHostages, MarkerIntentOutranksTheAudience)
{
    EXPECT_EQ(ObjectiveSource_ChooseHostages(HostageIntent::Marker, kCounterTerrorist, true), HostageSource::ServerMarker);
    EXPECT_EQ(ObjectiveSource_ChooseHostages(HostageIntent::Marker, kTerrorist, false), HostageSource::ServerMarker);
}

TEST(ObjectiveSourceHostages, TerroristsAreGatedBySight)
{
    EXPECT_EQ(ObjectiveSource_ChooseHostages(HostageIntent::Hostage, kTerrorist, true), HostageSource::Sight);
}

TEST(ObjectiveSourceHostages, CounterTerroristsFollowTheBroadcast)
{
    EXPECT_EQ(ObjectiveSource_ChooseHostages(HostageIntent::Hostage, kCounterTerrorist, true), HostageSource::Revealed);
    EXPECT_EQ(ObjectiveSource_ChooseHostages(HostageIntent::Hostage, kCounterTerrorist, false), HostageSource::Sight);
}

TEST(ObjectiveSourceHostages, SpectatorsAreNeverGated)
{
    EXPECT_EQ(ObjectiveSource_ChooseHostages(HostageIntent::Hostage, kSpectator, false), HostageSource::Revealed);
    EXPECT_EQ(ObjectiveSource_ChooseHostages(HostageIntent::Hostage, {RadarTeam::Terrorist, true}, false), HostageSource::Revealed);
}
