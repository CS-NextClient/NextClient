#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "RadarPolicy.h"

namespace
{
    std::vector<std::string> g_Log;

    void CaptureLog(const char* message)
    {
        g_Log.emplace_back(message);
    }

    bool LogContains(const char* needle)
    {
        for (const std::string& line : g_Log)
        {
            if (line.find(needle) != std::string::npos)
            {
                return true;
            }
        }

        return false;
    }

    constexpr float kRunSpeed = 320.0f; // stock sv_maxspeed: tolerated broadcast lag is 640 units
} // namespace

class RadarPolicyTest : public ::testing::Test
{
protected:
    RadarPolicy policy;

    void SetUp() override
    {
        g_Log.clear();
        policy.SetLogger(CaptureLog);
        policy.SetLogging(true);
        g_Log.clear(); // drop the snapshot printed by enabling logging
    }

    void Frame(float time)
    {
        policy.BeginFrame(time, kRunSpeed, true);
    }
};

TEST_F(RadarPolicyTest, AllyWithoutAnyBroadcastIsGated)
{
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});

    EXPECT_FALSE(policy.IsAllyRevealed(3));
    EXPECT_TRUE(LogContains("no radar broadcast"));
}

TEST_F(RadarPolicyTest, AllyTrackedByTheServerIsRevealed)
{
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    EXPECT_TRUE(policy.IsAllyRevealed(3));
    EXPECT_TRUE(LogContains("the server broadcasts it"));
}

TEST_F(RadarPolicyTest, BroadcastZeroedMidMapReadsAsFreeForAll)
{
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});
    ASSERT_TRUE(policy.IsAllyRevealed(3));

    Frame(11.0f);
    policy.ObserveAlly(3, true, {120.0f, 100.0f}, {});

    EXPECT_FALSE(policy.IsAllyRevealed(3));
    EXPECT_TRUE(LogContains("free-for-all"));
}

TEST_F(RadarPolicyTest, FrozenBroadcastGatesOnlyAfterTheGraceWindow)
{
    Frame(10.0f);
    policy.ObserveAlly(3, true, {1000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});
    ASSERT_TRUE(policy.IsAllyRevealed(3));

    Frame(11.0f);
    policy.ObserveAlly(3, true, {2000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});
    EXPECT_TRUE(policy.IsAllyRevealed(3));

    Frame(11.0f + RadarPolicy::kStaleTime);
    policy.ObserveAlly(3, true, {2000.0f + RadarPolicy::kStaleMoveDist, 0.0f}, {1000.0f, 0.0f, 0.0f});

    EXPECT_FALSE(policy.IsAllyRevealed(3));
    EXPECT_TRUE(LogContains("broadcast stuck"));
}

TEST_F(RadarPolicyTest, StandingAllyWithAStaleBroadcastIsNotGated)
{
    // Freezetime: the broadcast still holds last round's position and the server has no
    // reason to refresh it.
    for (int i = 0; i < 10; i++)
    {
        Frame(10.0f + (float)i);
        policy.ObserveAlly(3, true, {2000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});
    }

    EXPECT_TRUE(policy.IsAllyRevealed(3));
}

TEST_F(RadarPolicyTest, LagToleranceScalesWithTheServerRunSpeed)
{
    policy.BeginFrame(10.0f, 1000.0f, true);
    policy.ObserveAlly(3, true, {2000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});

    policy.BeginFrame(20.0f, 1000.0f, true);
    policy.ObserveAlly(3, true, {2000.0f, 0.0f}, {1000.0f, 0.0f, 0.0f});

    EXPECT_TRUE(policy.IsAllyRevealed(3));
}

TEST_F(RadarPolicyTest, ReturningBroadcastRevealsTheAllyAgain)
{
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});
    ASSERT_FALSE(policy.IsAllyRevealed(3));

    Frame(11.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    EXPECT_TRUE(policy.IsAllyRevealed(3));
}

TEST_F(RadarPolicyTest, AllyOutsideThePvsRidesOnTheBroadcastAlone)
{
    Frame(10.0f);
    policy.ObserveAlly(3, false, {}, {1000.0f, 0.0f, 0.0f});

    EXPECT_TRUE(policy.IsAllyRevealed(3));
}

TEST_F(RadarPolicyTest, SpectatingGatesNothing)
{
    policy.BeginFrame(10.0f, kRunSpeed, false);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});

    EXPECT_TRUE(policy.IsAllyRevealed(3));
    EXPECT_TRUE(g_Log.empty());
}

TEST_F(RadarPolicyTest, CvarForcesAllyGating)
{
    policy.SetForceAllyGating(true);
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    EXPECT_FALSE(policy.IsAllyRevealed(3));
    EXPECT_TRUE(LogContains("hud_radar_allies 0"));
}

TEST_F(RadarPolicyTest, ServerHintForcesAllyGating)
{
    policy.SetServerFfaHint(true);
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    EXPECT_FALSE(policy.IsAllyRevealed(3));
    EXPECT_TRUE(LogContains("serverinfo nc_ffa"));
}

TEST_F(RadarPolicyTest, BombMessageOnAMapWithoutTargetsIsAMarker)
{
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);

    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    EXPECT_TRUE(policy.is_marker_visible());
    EXPECT_TRUE(LogContains("no bomb targets"));
}

TEST_F(RadarPolicyTest, StationaryBombOnABombMapStaysABomb)
{
    policy.SetMapObjectives(2, 0);

    for (int i = 0; i < 5; i++)
    {
        float time = 10.0f + (float)i;
        policy.ObserveBomb({(std::uint32_t)(i + 1), true, {500.0f, 500.0f, 0.0f}, false, time}, time);
    }

    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Bomb);
    EXPECT_FALSE(policy.is_marker_visible());
    EXPECT_TRUE(LogContains("treated as the bomb"));
}

TEST_F(RadarPolicyTest, PlantedBombThatMovesIsAMarker)
{
    policy.SetMapObjectives(2, 0);
    policy.ObserveBomb({1, true, {500.0f, 500.0f, 0.0f}, true, 10.0f}, 10.0f);
    ASSERT_EQ(policy.get_bomb_intent(), BombIntent::Bomb);

    policy.ObserveBomb({2, true, {700.0f, 500.0f, 0.0f}, true, 11.0f}, 11.0f);

    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    EXPECT_TRUE(LogContains("planted bomb moved"));
}

TEST_F(RadarPolicyTest, BombTravellingTooFarForTooLongIsAMarker)
{
    policy.SetMapObjectives(2, 0);

    for (int i = 0; i < 5; i++)
    {
        float time = 10.0f + (float)i;
        policy.ObserveBomb({(std::uint32_t)(i + 1), true, {500.0f + 100.0f * i, 500.0f, 0.0f}, false, time}, time);
    }

    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    EXPECT_TRUE(LogContains("travelled"));
}

TEST_F(RadarPolicyTest, MarkerClassificationHoldsForTheRestOfTheMap)
{
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    ASSERT_EQ(policy.get_bomb_intent(), BombIntent::Marker);

    policy.SetMapObjectives(2, 0);
    policy.ObserveBomb({2, true, {100.0f, 100.0f, 0.0f}, false, 11.0f}, 11.0f);

    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Marker);
}

TEST_F(RadarPolicyTest, MarkerHidesWhenTheServerStopsRefreshingIt)
{
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    ASSERT_TRUE(policy.is_marker_visible());

    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f + RadarPolicy::kMarkerTimeout + 0.1f);

    EXPECT_FALSE(policy.is_marker_visible());
    EXPECT_TRUE(LogContains("no update for"));
}

TEST_F(RadarPolicyTest, ServerHintOutranksTheHeuristics)
{
    policy.SetMapObjectives(2, 0);
    policy.SetBombHint(MessageHint::Marker);

    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    EXPECT_TRUE(LogContains("serverinfo nc_bombmsg"));

    policy.ObserveBomb({1, true, {500.0f, 500.0f, 0.0f}, false, 10.0f}, 10.0f);
    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Marker);
}

TEST_F(RadarPolicyTest, HostageMessageOnAMapWithoutHostagesIsAMarker)
{
    policy.SetMapObjectives(2, 0);
    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);

    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    EXPECT_TRUE(policy.IsHostageMarkerVisible(1));
    EXPECT_TRUE(LogContains("the map has no hostages"));
}

TEST_F(RadarPolicyTest, HostageOnARescueMapStaysAHostage)
{
    policy.SetMapObjectives(0, 4);

    // CHostage's cadence: one position per second while the hostage walks.
    for (int i = 0; i < 6; i++)
    {
        float time = 10.0f + (float)i;
        policy.ObserveHostage({2, (std::uint32_t)(i + 1), time}, time);
    }

    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Hostage);
    EXPECT_FALSE(policy.IsHostageMarkerVisible(2));
    EXPECT_TRUE(LogContains("treated as a hostage"));
}

TEST_F(RadarPolicyTest, HostageIndexPastTheMapsHostagesIsAMarker)
{
    policy.SetMapObjectives(0, 4);
    policy.ObserveHostage({9, 1, 10.0f}, 10.0f);

    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    EXPECT_TRUE(LogContains("past the map's 4 hostage(s)"));
}

TEST_F(RadarPolicyTest, HostageUpdatesFasterThanTheServerCanSendAreAMarker)
{
    policy.SetMapObjectives(0, 4);

    for (int i = 0; i < 12; i++)
    {
        float time = 10.0f + 0.3f * (float)i;
        policy.ObserveHostage({1, (std::uint32_t)(i + 1), time}, time);
    }

    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    EXPECT_TRUE(LogContains("above the rate CHostage can send"));
}

TEST_F(RadarPolicyTest, HostageMarkerHidesWhenUpdatesStop)
{
    policy.SetMapObjectives(2, 0);
    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);
    ASSERT_TRUE(policy.IsHostageMarkerVisible(1));

    policy.ObserveHostage({1, 1, 10.0f}, 10.0f + RadarPolicy::kMarkerTimeout + 0.1f);

    EXPECT_FALSE(policy.IsHostageMarkerVisible(1));
    EXPECT_TRUE(LogContains("server marker 1 hidden"));
}

TEST_F(RadarPolicyTest, HostagesAreGatedWhenTheServerSendsNoBroadcast)
{
    policy.SetMapObjectives(0, 4);
    policy.ObserveHostageBroadcast(true, 0);

    EXPECT_FALSE(policy.are_hostages_revealed());
    EXPECT_TRUE(LogContains("sends no hostage broadcast"));

    policy.ObserveHostageBroadcast(true, 2);

    EXPECT_TRUE(policy.are_hostages_revealed());
}

TEST_F(RadarPolicyTest, HostageGatingNeverAppliesOffTheCtSide)
{
    // Terrorists are never sent HostagePos, so its absence says nothing.
    policy.SetMapObjectives(0, 4);
    policy.ObserveHostageBroadcast(false, 0);

    EXPECT_TRUE(policy.are_hostages_revealed());
    EXPECT_TRUE(g_Log.empty());
}

TEST_F(RadarPolicyTest, HostageGatingNeverAppliesOnAMapWithoutHostages)
{
    policy.SetMapObjectives(2, 0);
    policy.ObserveHostageBroadcast(true, 0);

    EXPECT_TRUE(policy.are_hostages_revealed());
    EXPECT_TRUE(g_Log.empty());
}

TEST_F(RadarPolicyTest, HostageHintOutranksTheHeuristics)
{
    policy.SetMapObjectives(0, 4);
    policy.SetHostageHint(MessageHint::Marker);

    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    EXPECT_TRUE(LogContains("serverinfo nc_hostagemsg"));

    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);
    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
}

TEST_F(RadarPolicyTest, LevelChangeForgetsTheDecisions)
{
    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    policy.ObserveHostage({1, 1, 10.0f}, 10.0f);
    Frame(10.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});
    ASSERT_EQ(policy.get_bomb_intent(), BombIntent::Marker);
    ASSERT_EQ(policy.get_hostage_intent(), HostageIntent::Marker);
    ASSERT_FALSE(policy.IsAllyRevealed(3));

    policy.OnLevelChange();
    Frame(11.0f);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {100.0f, 100.0f, 0.0f});

    EXPECT_EQ(policy.get_bomb_intent(), BombIntent::Unknown);
    EXPECT_EQ(policy.get_hostage_intent(), HostageIntent::Unknown);
    EXPECT_TRUE(policy.IsAllyRevealed(3));
}

TEST(RadarPolicyLogging, DecisionsAreSilentUntilDebugIsEnabled)
{
    g_Log.clear();
    RadarPolicy policy;
    policy.SetLogger(CaptureLog);

    policy.SetMapObjectives(0, 0);
    policy.ObserveBomb({1, true, {100.0f, 100.0f, 0.0f}, false, 10.0f}, 10.0f);
    policy.BeginFrame(10.0f, kRunSpeed, true);
    policy.ObserveAlly(3, true, {100.0f, 100.0f}, {});

    EXPECT_TRUE(g_Log.empty());

    policy.SetLogging(true);

    EXPECT_TRUE(LogContains("1 ally(ies) gated"));
    EXPECT_TRUE(LogContains("bomb message = a server marker"));
}
