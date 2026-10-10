#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <tuple>
#include <gtest/gtest.h>
#include <nextclient/runtime.h>
#include <windows.h>
#include "catalog.h"
#include "plugin_game_state.h"
#include "tracker.h"

using life_stats::Tracker;
TEST(LifeStats, RecordsDamageAndKillsAndWaitsForFinalDamageUpdate)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    tracker.Damage(25, 10, 2, 0.5);
    tracker.Death(1, 2, "Alice", "ak47", true, 1);
    tracker.Death(3, 1, "Bob", "deagle", false, 2);
    tracker.Damage(75, 5, 2, 2.05);
    tracker.Tick(0.1);
    EXPECT_TRUE(tracker.TakeReport().empty());
    tracker.Tick(0.16);
    const auto report = tracker.TakeReport();
    ASSERT_EQ(report.chat.size(), 3u);
    EXPECT_EQ(report.chat[0], "[Life Stats] Life ended. Damage taken: 100 HP, 15 armor. Kills: 1.");
    EXPECT_EQ(report.chat[1], "[Life Stats] Killed by Bob (deagle).");
    EXPECT_EQ(report.chat[2], "[Life Stats] Killed Alice (ak47, headshot).");
    ASSERT_EQ(report.console.size(), 4u);
    EXPECT_EQ(report.console[1], "[Life Stats] Bullet damage -25 HP, -10 armor (-00:01.500)");
    EXPECT_EQ(report.console[2], "[Life Stats] Killed Alice (ak47, headshot) (-00:01.000)");
    EXPECT_EQ(report.console[3], "[Life Stats] Damage from Bob (deagle) -75 HP, -5 armor (Death)");
    tracker.RoundEnd(3);
    tracker.Tick(1);
    EXPECT_TRUE(tracker.TakeReport().empty());
}
TEST(LifeStats, FatalOverkillAndMillisecondOffsetsUseHealthBeforeDamage)
{
    Tracker tracker;
    tracker.Observe(1, true, 1, 100);
    // The server sends the updated Health before the corresponding Damage.
    tracker.Health(4, 10);
    tracker.Damage(96, 0, 64, 10);
    tracker.Health(0, 15.405);
    tracker.Death(1, 1, "Self", "grenade", false, 15.405);
    tracker.Damage(95, 0, 64, 15.405);
    tracker.Tick(0.3);
    auto report = tracker.TakeReport();
    ASSERT_EQ(report.console.size(), 3u);
    EXPECT_EQ(report.console[0], "[Life Stats] Life ended. Damage taken: 100 HP, 0 armor. Overkill: 91 HP. Kills: 0.");
    EXPECT_EQ(report.console[1], "[Life Stats] Explosion damage -96 HP (-00:05.405)");
    EXPECT_EQ(report.console[2], "[Life Stats] Self-inflicted damage (grenade) -4 HP (91 HP overkill) (Death)");
}
TEST(LifeStats, HealingIsIncludedWithoutTurningItIntoOverkill)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    tracker.Damage(80, 0, 2, 1);
    tracker.Health(20, 1);
    tracker.Health(50, 2);
    tracker.Damage(50, 0, 2, 3);
    tracker.Death(2, 1, "Bob", "ak47", false, 3);
    tracker.Tick(0.3);
    auto report = tracker.TakeReport();
    EXPECT_NE(report.console[0].find("130 HP"), std::string::npos);
    EXPECT_EQ(report.console[0].find("Overkill"), std::string::npos);
}
TEST(LifeStats, NearbyNonfatalHitDoesNotBorrowTheCauseOfALaterSuicide)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    tracker.Damage(30, 0, 0, 1);
    tracker.Death(1, 1, "Self", "world", false, 1.1);
    tracker.Tick(0.3);
    const auto report = tracker.TakeReport();
    ASSERT_EQ(report.console.size(), 3u);
    EXPECT_EQ(report.console[1], "[Life Stats] Damage taken -30 HP (-00:00.100)");
    EXPECT_EQ(report.console[2], "[Life Stats] Self-inflicted death (world).");
}
TEST(LifeStats, DeathMessageDoesNotTreatStaleAliveSnapshotAsRespawn)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    tracker.Death(2, 1, "Bob", "ak47", false, 1);
    tracker.Observe(1, true, 1.1, 100);
    tracker.Damage(100, 0, 2, 1.1);
    tracker.Tick(0.3);
    auto report = tracker.TakeReport();
    ASSERT_EQ(report.chat.size(), 2u);
    EXPECT_NE(report.chat[0].find("100 HP"), std::string::npos);
}
TEST(LifeStats, HudResetDoesNotConfirmRespawnFromStaleAliveSnapshot)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    tracker.Death(2, 1, "Bob", "ak47", false, 1);
    tracker.ResetHud();
    tracker.Observe(1, true, 1.1, 100);
    EXPECT_TRUE(tracker.TakeReport().empty());
    tracker.Damage(100, 0, 2, 1.1);
    tracker.Tick(0.3);
    const auto report = tracker.TakeReport();
    ASSERT_EQ(report.console.size(), 2u);
    EXPECT_NE(report.console[0].find("100 HP"), std::string::npos);
    EXPECT_NE(report.console[1].find("Damage from Bob (ak47) -100 HP (Death)"), std::string::npos);
}
TEST(LifeStats, RoundReportIsOnceAndNextLifeStartsClean)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    tracker.Damage(10, 1, 2, 0.125);
    tracker.RoundEnd(1);
    tracker.Tick(0.3);
    auto first = tracker.TakeReport();
    ASSERT_EQ(first.console.size(), 2u);
    EXPECT_EQ(first.console[1], "[Life Stats] Bullet damage -10 HP, -1 armor (-00:00.875)");
    tracker.Observe(1, true, 2, 90);
    tracker.RoundEnd(2);
    tracker.Tick(0.3);
    EXPECT_TRUE(tracker.TakeReport().empty());
    tracker.NewRound();
    tracker.ResetHud();
    tracker.Observe(1, true, 4, 100);
    tracker.RoundEnd(5);
    tracker.Tick(0.3);
    auto report = tracker.TakeReport();
    ASSERT_EQ(report.chat.size(), 1u);
    EXPECT_EQ(report.chat[0], "[Life Stats] Round ended. Damage taken: 0 HP, 0 armor. Kills: 0.");
}
TEST(LifeStats, NewRoundDoesNotInventALateReportForAMissingRoundEnd)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    tracker.Damage(20, 0, 0, 0.5);
    tracker.NewRound();
    EXPECT_TRUE(tracker.TakeReport().empty());
    tracker.ResetHud();
    tracker.Observe(1, true, 1, 100);
    tracker.ResetHud();
    EXPECT_TRUE(tracker.TakeReport().empty());
}
TEST(LifeStats, SpectatorsUnrelatedDeathsAndWorldDeathsDoNotInventAttribution)
{
    Tracker tracker;
    tracker.Observe(1, false, 0);
    tracker.ResetHud();
    tracker.Observe(1, false, 1);
    tracker.Damage(100, 0, 0, 1);
    tracker.RoundEnd(1);
    tracker.Tick(0.3);
    EXPECT_TRUE(tracker.TakeReport().empty());
    tracker.NewRound();
    tracker.Observe(1, true, 3);
    tracker.Death(2, 3, "Other", "ak47", true, 3);
    tracker.Death(0, 1, "", "worldspawn", false, 4);
    tracker.Tick(0.3);
    auto report = tracker.TakeReport();
    ASSERT_EQ(report.chat.size(), 1u);
    EXPECT_NE(report.chat[0].find("Kills: 0"), std::string::npos);
    EXPECT_EQ(report.chat[0].find("unknown"), std::string::npos);
    EXPECT_EQ(report.chat[0].find("unavailable"), std::string::npos);
}
TEST(LifeStats, SuicidesAndPosthumousKillsDoNotCountAsKillsDuringLife)
{
    Tracker tracker;
    tracker.Observe(1, true, 0);
    tracker.Death(1, 1, "Self", "hegrenade", false, 1);
    tracker.Death(1, 2, "Late", "hegrenade", false, 1.1);
    tracker.Tick(0.3);
    auto report = tracker.TakeReport();
    ASSERT_EQ(report.chat.size(), 2u);
    EXPECT_NE(report.chat[0].find("Kills: 0"), std::string::npos);
    EXPECT_EQ(report.chat[1], "[Life Stats] Self-inflicted death (hegrenade).");
    tracker.Reset();
    tracker.Tick(0.3);
    EXPECT_TRUE(tracker.TakeReport().empty());
}
TEST(LifeStats, NamesAreCapturedAndCannotInjectChatLinesOrFormatting)
{
    Tracker tracker;
    tracker.Observe(1, true, 0);
    tracker.Death(1, 2, "Alice\n%s1\x03", "ak47", false, 1);
    tracker.RoundEnd(2);
    tracker.Tick(0.3);
    auto report = tracker.TakeReport();
    ASSERT_EQ(report.chat.size(), 2u);
    EXPECT_EQ(report.chat[1].find('\n'), std::string::npos);
    EXPECT_EQ(report.chat[1].find('%'), std::string::npos);
    EXPECT_EQ(report.chat[1].find('\x03'), std::string::npos);
    EXPECT_LT(report.chat[1].size(), 190u);
    EXPECT_EQ(life_stats::DisplayText("абв", 3), "а");
}

namespace
{
    std::vector<std::string> console, chat;
    int network_calls;
    int g_ChatCalls{};
    bool alive, spectator, accept_chat;
    int current_health;
    NcSession current_session;
    std::array<std::string, 33> g_Names;
    bool g_InfoAvailable{};
    int32_t Player(NcPlayerState* value)
    {
        *value = {};
        value->size = sizeof(*value);
        value->index = 1;
        value->flags = NC_PLAYER_VALID | (spectator ? 0 : NC_PLAYER_ACTIVE);
        value->health = alive ? current_health : 0;
        return 1;
    }
    int32_t Session(NcSession* value)
    {
        *value = current_session;
        return 1;
    }
    int32_t Info(int32_t index, NcPlayerInfo* value)
    {
        if (!g_InfoAvailable)
        {
            return 0;
        }
        *value = {};
        value->size = sizeof(*value);
        value->index = index;
        strcpy_s(value->name, g_Names[index].c_str());
        return 1;
    }
    void Print(const char* text)
    {
        console.emplace_back(text);
    }
    int32_t Chat(const char* text)
    {
        ++g_ChatCalls;
        if (!accept_chat)
            return 0;
        chat.emplace_back(text);
        return 1;
    }
    int32_t Send(const char*, int32_t)
    {
        ++network_calls;
        return 1;
    }
} // namespace
class LifeStatsPlugin : public ::testing::TestWithParam<const char*>
{
protected:
    std::filesystem::path dir;
    void SetUp() override
    {
        console.clear();
        chat.clear();
        network_calls = 0;
        g_ChatCalls = 0;
        alive = accept_chat = true;
        current_health = 100;
        g_InfoAvailable = true;
        g_Names.fill("Bob");
        g_Names[2] = "Alice";
        spectator = false;
        current_session = {};
        current_session.size = sizeof(current_session);
        current_session.flags = NC_SESSION_CONNECTED | NC_SESSION_IN_GAME;
        current_session.max_clients = 3;
        current_session.frame_time = 0.01f;
        dir = std::filesystem::temp_directory_path() /
              (L"life-stats-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(dir / L"plugins");
        std::filesystem::copy_file(GetParam(), dir / L"plugins/life_stats.dll");
        NcClientServices services{};
        services.get_player = Player;
        services.get_player_info = Info;
        services.get_session = Session;
        services.console_print = Print;
        services.chat_print = Chat;
        services.send_chat = Send;
        nc_runtime_bind_client(&services);
        nc_runtime_start(dir.c_str(), 0);
        auto rows = plugins::parse(nc_runtime_catalog()).at("plugins");
        ASSERT_EQ(rows.at(0).at("permissions"), tao::json::value::array({"chat.print"}));
        rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
        ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
        nc_runtime_stop();
        nc_runtime_start(dir.c_str(), 0);
        ASSERT_TRUE(plugins::parse(nc_runtime_catalog()).at("plugins").at(0).at("running").get_boolean());
        Frame();
    }
    void TearDown() override
    {
        nc_runtime_stop();
        nc_runtime_bind_client(nullptr);
        std::filesystem::remove_all(dir);
    }
    void Frame(int count = 1)
    {
        for (int i = 0; i < count; ++i)
        {
            current_session.time += current_session.frame_time;
            nc_runtime_frame(&current_session);
        }
    }
    void SetOutput(int value)
    {
        const tao::json::value edits =
            tao::json::value::array({tao::json::value{{"owner", "org.nextclient.life_stats"}, {"id", "output"}, {"value", value}}});
        ASSERT_STREQ(nc_runtime_settings(tao::json::to_string(edits).c_str()), "");
    }
    void Restart()
    {
        nc_runtime_stop();
        nc_runtime_start(dir.c_str(), 0);
        ASSERT_TRUE(plugins::parse(nc_runtime_catalog()).at("plugins").at(0).at("running").get_boolean());
        Frame();
    }
    void Backlog(int count)
    {
        for (int i = 0; i < count; ++i)
            nc_runtime_event("player.joined", R"({"index":2,"name":"Alice"})");
    }
};
TEST_P(LifeStatsPlugin, SharedOutputSettingDefaultsToConsoleWithoutChatCalls)
{
    const auto ui = plugins::parse(nc_runtime_ui());
    ASSERT_EQ(ui.get_array().size(), 1u);
    EXPECT_TRUE(ui.at(0).at("tabs").get_array().empty());
    const auto& controls = ui.at(0).at("controls").get_array();
    ASSERT_EQ(controls.size(), 1u);
    EXPECT_EQ(controls[0].at("id"), "output");
    EXPECT_EQ(controls[0].at("tab"), NC_PLUGIN_SETTINGS_TAB);
    EXPECT_EQ(controls[0].at("kind").as<unsigned>(), NC_CHOICE);
    EXPECT_EQ(controls[0].at("value"), 0);
    EXPECT_EQ(controls[0].at("choices_en"), "Console\nConsole + Chat");
    EXPECT_EQ(controls[0].at("choices_ru"), "Консоль\nКонсоль + чат");
    nc_runtime_event("round.end", "{}");
    Frame(100);
    ASSERT_EQ(console.size(), 1u);
    EXPECT_TRUE(chat.empty());
    EXPECT_EQ(g_ChatCalls, 0);
    EXPECT_EQ(network_calls, 0);
}

TEST_P(LifeStatsPlugin, BothOutputChoicesPersistAcrossReload)
{
    SetOutput(1);
    Restart();
    EXPECT_EQ(plugins::parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
    nc_runtime_event("round.end", "{}");
    Frame(100);
    ASSERT_EQ(console.size(), 1u);
    ASSERT_EQ(chat.size(), 1u);
    SetOutput(0);
    Restart();
    EXPECT_EQ(plugins::parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 0);
    const int calls = g_ChatCalls;
    nc_runtime_event("round.end", "{}");
    Frame(100);
    EXPECT_EQ(console.size(), 2u);
    EXPECT_EQ(chat.size(), 1u);
    EXPECT_EQ(g_ChatCalls, calls);
    EXPECT_EQ(network_calls, 0);
}

TEST_P(LifeStatsPlugin, ConsoleModeDropsChatThatCouldNotBeDisplayed)
{
    SetOutput(1);
    accept_chat = false;
    nc_runtime_event("round.end", "{}");
    Frame(30);
    ASSERT_EQ(console.size(), 1u);
    ASSERT_GT(g_ChatCalls, 0);
    EXPECT_TRUE(chat.empty());
    SetOutput(0);
    const int calls = g_ChatCalls;
    accept_chat = true;
    Frame(100);
    EXPECT_EQ(g_ChatCalls, calls);
    EXPECT_TRUE(chat.empty());
    SetOutput(1);
    Frame(100);
    EXPECT_TRUE(chat.empty());
    EXPECT_EQ(console.size(), 1u);
}

TEST_P(LifeStatsPlugin, EnablingChatDoesNotReplayEarlierConsoleReports)
{
    nc_runtime_event("round.end", "{}");
    Frame(30);
    ASSERT_EQ(console.size(), 1u);
    SetOutput(1);
    Frame(100);
    EXPECT_TRUE(chat.empty());
    nc_runtime_event("round.start", "{}");
    Frame();
    nc_runtime_event("round.end", "{}");
    Frame(100);
    EXPECT_EQ(console.size(), 2u);
    EXPECT_EQ(chat.size(), 1u);
}

TEST_P(LifeStatsPlugin, DeferredHealthDoesNotTurnAnOlderUpdateIntoHealing)
{
    Backlog(129);
    nc_runtime_event("player.health", R"({"health":75,"time":1})");
    nc_runtime_event("player.damage", R"({"health":25,"armor":0,"time":1})");
    nc_runtime_event("player.health", R"({"health":50,"time":2})");
    nc_runtime_event("player.damage", R"({"health":25,"armor":0,"time":2})");
    current_health = 50;
    Frame(10);
    nc_runtime_event("player.health", R"({"health":0,"time":3})");
    nc_runtime_event("player.damage", R"({"health":100,"armor":0,"time":3})");
    nc_runtime_event("player.death", R"({"killer":2,"victim":1,"weapon":"ak47","headshot":false,"time":3})");
    alive = false;
    Frame(30);
    ASSERT_FALSE(console.empty());
    EXPECT_NE(console[0].find("100 HP, 0 armor. Overkill: 50 HP"), std::string::npos);
}
TEST_P(LifeStatsPlugin, DeferredFatalEventsArriveBeforeReportTimerStarts)
{
    current_session.frame_time = 0.3f;
    Backlog(600);
    nc_runtime_event("player.health", R"({"health":0,"time":1})");
    nc_runtime_event("player.damage", R"({"health":100,"armor":0,"time":1})");
    nc_runtime_event("player.death", R"({"killer":2,"victim":1,"weapon":"ak47","headshot":false,"time":1})");
    alive = false;
    Frame();
    EXPECT_TRUE(console.empty());
    Frame(12);
    ASSERT_FALSE(console.empty());
    EXPECT_NE(console[0].find("100 HP"), std::string::npos);
    EXPECT_NE(console.back().find("Damage from Alice"), std::string::npos);
}
TEST_P(LifeStatsPlugin, OverflowInvalidatesLifeUntilBacklogDrainsAndNewBoundaryArrives)
{
    nc_runtime_event("player.damage", R"({"health":25,"armor":0,"time":1})");
    Frame();
    Backlog(100);
    nc_runtime_event("hud.reset", "{}"); // A surviving reset may precede lost events.
    Backlog(2100);
    Frame(50);
    ASSERT_EQ(console.size(), 1u);
    EXPECT_NE(console[0].find("Events were lost"), std::string::npos);
    EXPECT_TRUE(chat.empty());
    EXPECT_EQ(g_ChatCalls, 0);
    nc_runtime_event("round.end", "{}");
    Frame(30);
    EXPECT_EQ(console.size(), 1u);
    nc_runtime_event("hud.reset", "{}");
    Frame();
    nc_runtime_event("player.damage", R"({"health":10,"armor":0,"time":3})");
    nc_runtime_event("round.end", R"({"time":4})");
    Frame(30);
    ASSERT_EQ(console.size(), 3u);
    EXPECT_NE(console[1].find("10 HP"), std::string::npos);
}
TEST_P(LifeStatsPlugin, ReportsDeathBeforeRespawnWithColoredPrivateSummaryAndConsoleTimeline)
{
    SetOutput(1);
    nc_runtime_event("player.damage", R"({"health":25,"armor":10,"bits":2,"time":0.01})");
    nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"ak47","headshot":true,"time":0.01})");
    nc_runtime_event("player.death", R"({"killer":3,"victim":1,"weapon":"deagle","headshot":false,"time":0.02})");
    nc_runtime_event("player.damage", R"({"health":75,"armor":5,"bits":2,"time":0.02})");
    alive = false;
    current_session.flags = NC_SESSION_CONNECTED; // Dead/spectating still allows local chat.
    Frame(20);
    EXPECT_TRUE(console.empty());
    Frame(10);
    ASSERT_EQ(console.size(), 4u);
    ASSERT_EQ(chat.size(), 1u); // First line within 0.3 seconds, before another round.
    EXPECT_TRUE(console[0].starts_with("\x04[Life Stats]\x01"));
    EXPECT_EQ(console[0].find("org.nextclient"), std::string::npos);
    EXPECT_NE(console[0].find("100 HP, 15 armor. Kills: 1"), std::string::npos);
    EXPECT_NE(console[3].find("Damage from Bob (deagle) -75 HP, -5 armor (Death)"), std::string::npos);
    Frame(150);
    ASSERT_EQ(chat.size(), 3u);
    EXPECT_TRUE(chat[0].starts_with("\x04[Life Stats]\x01"));
    EXPECT_NE(chat[1].find("Killed by Bob"), std::string::npos);
    EXPECT_NE(chat[2].find("Killed Alice"), std::string::npos);
    EXPECT_EQ(network_calls, 0);
    nc_runtime_event("round.end", R"({"reason":"#CTs_Win"})");
    Frame(30);
    EXPECT_EQ(console.size(), 4u);
}
TEST_P(LifeStatsPlugin, HudRefreshPreservesDamageKillsAndRemainingHealth)
{
    nc_runtime_event("player.health", R"({"health":70,"time":1})");
    nc_runtime_event("player.damage", R"({"health":30,"armor":5,"bits":2,"time":1})");
    nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"ak47","headshot":false,"time":2})");
    current_health = 70;
    Frame();
    nc_runtime_event("hud.reset", R"({"time":3})");
    nc_runtime_event("player.health", R"({"health":70,"time":3})");
    Frame();
    EXPECT_TRUE(console.empty());
    nc_runtime_event("player.health", R"({"health":0,"time":4})");
    nc_runtime_event("player.death", R"({"killer":3,"victim":1,"weapon":"deagle","headshot":false,"time":4})");
    nc_runtime_event("player.damage", R"({"health":80,"armor":0,"bits":2,"time":4})");
    alive = false;
    Frame(30);
    ASSERT_EQ(console.size(), 4u);
    EXPECT_NE(console[0].find("100 HP, 5 armor. Overkill: 10 HP. Kills: 1."), std::string::npos);
    EXPECT_NE(console[1].find("-30 HP, -5 armor"), std::string::npos);
    EXPECT_NE(console[2].find("Killed Alice (ak47)"), std::string::npos);
    EXPECT_NE(console[3].find("Damage from Bob (deagle) -70 HP (10 HP overkill) (Death)"), std::string::npos);
}
TEST_P(LifeStatsPlugin, RapidRespawnKeepsPendingDeathReportAndStartsFreshLife)
{
    nc_runtime_event("player.health", R"({"health":75,"time":1})");
    nc_runtime_event("player.damage", R"({"health":25,"armor":0,"bits":2,"time":1})");
    nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"ak47","headshot":false,"time":2})");
    current_health = 75;
    Frame();
    nc_runtime_event("player.health", R"({"health":0,"time":3})");
    nc_runtime_event("player.death", R"({"killer":3,"victim":1,"weapon":"deagle","headshot":false,"time":3})");
    nc_runtime_event("player.damage", R"({"health":90,"armor":0,"bits":2,"time":3})");
    nc_runtime_event("hud.reset", R"({"time":4})");
    nc_runtime_event("player.health", R"({"health":100,"time":4})");
    nc_runtime_event("player.health", R"({"health":90,"time":5})");
    nc_runtime_event("player.damage", R"({"health":10,"armor":0,"bits":2,"time":5})");
    current_health = 90;
    Frame();
    ASSERT_EQ(console.size(), 4u);
    EXPECT_NE(console[0].find("100 HP, 0 armor. Overkill: 15 HP. Kills: 1."), std::string::npos);
    EXPECT_NE(console[3].find("Damage from Bob (deagle) -75 HP (15 HP overkill) (Death)"), std::string::npos);
    nc_runtime_event("round.end", R"({"time":6})");
    Frame(30);
    ASSERT_EQ(console.size(), 6u);
    EXPECT_NE(console[4].find("Round ended. Damage taken: 10 HP, 0 armor. Kills: 0."), std::string::npos);
    EXPECT_NE(console[5].find("-10 HP"), std::string::npos);
}
TEST_P(LifeStatsPlugin, DeadHudRefreshPreservesPendingDamageWithoutDuplicateReports)
{
    nc_runtime_event("player.health", R"({"health":0,"time":1})");
    nc_runtime_event("player.death", R"({"killer":2,"victim":1,"weapon":"ak47","headshot":false,"time":1})");
    alive = false;
    Frame();
    nc_runtime_event("hud.reset", R"({"time":1.05})");
    nc_runtime_event("player.health", R"({"health":0,"time":1.05})");
    nc_runtime_event("player.damage", R"({"health":100,"armor":0,"bits":2,"time":1.05})");
    Frame();
    EXPECT_TRUE(console.empty());
    Frame(30);
    ASSERT_EQ(console.size(), 2u);
    EXPECT_NE(console[0].find("100 HP, 0 armor. Kills: 0."), std::string::npos);
    EXPECT_NE(console[1].find("Damage from Alice (ak47) -100 HP (Death)"), std::string::npos);
    nc_runtime_event("hud.reset", R"({"time":2})");
    nc_runtime_event("player.health", R"({"health":0,"time":2})");
    Frame(30);
    nc_runtime_event("round.end", R"({"time":3})");
    Frame(30);
    EXPECT_EQ(console.size(), 2u);
}
TEST_P(LifeStatsPlugin, HudRefreshDoesNotReopenReportedRound)
{
    nc_runtime_event("player.health", R"({"health":90,"time":1})");
    nc_runtime_event("player.damage", R"({"health":10,"armor":0,"bits":2,"time":1})");
    nc_runtime_event("round.end", R"({"time":2})");
    current_health = 90;
    Frame(30);
    ASSERT_EQ(console.size(), 2u);
    nc_runtime_event("hud.reset", R"({"time":3})");
    nc_runtime_event("player.health", R"({"health":90,"time":3})");
    Frame(30);
    nc_runtime_event("round.end", R"({"time":4})");
    Frame(30);
    EXPECT_EQ(console.size(), 2u);
    nc_runtime_event("round.start", R"({"time":5})");
    nc_runtime_event("hud.reset", R"({"time":5})");
    nc_runtime_event("player.health", R"({"health":100,"time":5})");
    nc_runtime_event("round.end", R"({"time":6})");
    current_health = 100;
    Frame(30);
    ASSERT_EQ(console.size(), 3u);
    EXPECT_NE(console[2].find("Round ended. Damage taken: 0 HP, 0 armor. Kills: 0."), std::string::npos);
}
TEST_P(LifeStatsPlugin, SpectatorHudHealthAfterRefreshDoesNotStartLife)
{
    spectator = true;
    alive = false;
    nc_runtime_event("map.changed", R"({"old":"old","map":"new"})");
    Frame();
    nc_runtime_event("hud.reset", R"({"time":1})");
    nc_runtime_event("player.health", R"({"health":100,"time":1})");
    nc_runtime_event("player.damage", R"({"health":25,"armor":0,"bits":2,"time":2})");
    nc_runtime_event("round.end", R"({"time":3})");
    Frame(30);
    EXPECT_TRUE(console.empty());
    EXPECT_TRUE(chat.empty());
}
TEST_P(LifeStatsPlugin, ReportsSurvivorBeforeNextRoundAndResetsOnMapChange)
{
    nc_runtime_event("player.damage", R"({"health":30,"armor":0,"time":0.01})");
    nc_runtime_event("round.end", R"({"reason":"#CTs_Win","time":1.125})");
    Frame(30);
    ASSERT_EQ(console.size(), 2u);
    EXPECT_NE(console[0].find("Round ended. Damage taken: 30 HP"), std::string::npos);
    EXPECT_NE(console[1].find("(-00:01.115)"), std::string::npos);
    nc_runtime_event("round.start", "{}");
    Frame(30);
    EXPECT_EQ(console.size(), 2u);
    nc_runtime_event("map.changed", R"({"old":"old","map":"new"})");
    Frame();
    nc_runtime_event("round.end", "{}");
    Frame(30);
    ASSERT_EQ(console.size(), 3u);
    EXPECT_NE(console[2].find("0 HP, 0 armor"), std::string::npos);
}
TEST_P(LifeStatsPlugin, RetriesLocalChatWhenTheHudIsTemporarilyUnavailable)
{
    SetOutput(1);
    accept_chat = false;
    nc_runtime_event("round.end", "{}");
    Frame(30);
    EXPECT_TRUE(chat.empty());
    ASSERT_EQ(console.size(), 1u);
    accept_chat = true;
    Frame(100);
    ASSERT_EQ(chat.size(), 1u);
    EXPECT_EQ(console.size(), 1u);
}

TEST_P(LifeStatsPlugin, OptionalDeathDetailsEnrichConsoleAndCountOnlyReportedLocalAssists)
{
    SetOutput(1);
    // Remote scoreboard health must not end the local life.
    nc_runtime_event("player.health", R"({"player":2,"reported_health":0,"time":0.01})");
    nc_runtime_event("player.health", R"({"player":2,"reported_health":null,"time":0.01})");
    nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"awp","headshot":false,"time":0.02,
        "assister":3,"kill_details":{"noscope":true,"penetrated":true,"domination_began":true,"domination":true}})");
    nc_runtime_event("player.death", R"({"killer":3,"victim":2,"weapon":"ak47","headshot":true,"time":0.03,
        "assister":1,"kill_details":{"assisted_flash":true}})");
    nc_runtime_event("player.death", R"({"killer":2,"victim":1,"weapon":"deagle","headshot":false,"time":0.04,
        "assister":3,"kill_details":{"through_smoke":true,"killer_blind":true,"in_air":true,"revenge":true}})");
    alive = false;
    Frame(240);
    ASSERT_EQ(console.size(), 4u);
    EXPECT_NE(console[0].find("Kills: 1. Assists: 1."), std::string::npos);
    EXPECT_NE(console[1].find("no-scope, through wall, domination began, assisted by Bob"), std::string::npos);
    EXPECT_EQ(console[1].find("domination began, domination"), std::string::npos);
    EXPECT_NE(console[2].find("Assisted in killing Alice (ak47, headshot) [flash assist]"), std::string::npos);
    EXPECT_NE(console[3].find("blinded killer, through smoke, revenge, airborne killer, assisted by Bob"), std::string::npos);
    ASSERT_EQ(chat.size(), 4u);
    EXPECT_NE(chat[3].find("Assisted in killing Alice (ak47, headshot)."), std::string::npos);
    for (const auto& line : chat)
    {
        EXPECT_EQ(line.find("no-scope"), std::string::npos);
        EXPECT_LT(line.size(), 190u);
    }
    EXPECT_EQ(network_calls, 0);
}

TEST(LifeStats, AssistCountsDoNotLeakAcrossLivesOrIncludeSuicidesAndPosthumousAssists)
{
    Tracker tracker;
    tracker.Observe(1, true, 0, 100);
    life_stats::DeathDetails extras{1, "Self", {"flash assist"}};
    tracker.Death(2, 2, "Alice", "grenade", false, 1, extras);
    tracker.Death(0, 2, "Alice", "world", false, 1, extras);
    tracker.Death(2, 3, "Bob", "ak47", false, 2, extras);
    tracker.Death(2, 1, "Alice", "ak47", false, 3);
    tracker.Death(2, 3, "Bob", "ak47", false, 3.1, extras);
    tracker.Tick(0.3);
    const auto report = tracker.TakeReport();
    ASSERT_FALSE(report.console.empty());
    EXPECT_NE(report.console[0].find("Kills: 0. Assists: 1."), std::string::npos);
    tracker.NewRound();
    tracker.Observe(1, true, 5, 100);
    tracker.RoundEnd(6);
    tracker.Tick(0.3);
    const auto next = tracker.TakeReport();
    ASSERT_FALSE(next.console.empty());
    EXPECT_EQ(next.console[0].find("Assists"), std::string::npos);
}

TEST_P(LifeStatsPlugin, VipAndEscapeWireOutcomesReportSurvivorsBeforeNextRound)
{
    PluginGameState state;
    for (const auto* reason :
         {"#VIP_Not_Escaped", "#Terrorists_Escaped", "#CTs_PreventEscape", "#Escaping_Terrorists_Neutralized", "#Terrorists_Not_Escaped"})
    {
        SCOPED_TRACE(reason);
        const auto before = console.size();
        std::string packet(1, 4);
        packet += reason;
        packet.push_back(0);
        for (const auto& event : state.Message("TextMsg", packet.data(), packet.size(), current_session.time))
            nc_runtime_event(event.name.c_str(), tao::json::to_string(event.data).c_str());
        Frame(30);
        ASSERT_EQ(console.size(), before + 1);
        EXPECT_NE(console.back().find("Round ended."), std::string::npos);
        const char round[]{0, 0};
        for (const auto& event : state.Message("HLTV", round, sizeof(round), current_session.time))
            nc_runtime_event(event.name.c_str(), tao::json::to_string(event.data).c_str());
        Frame();
        EXPECT_EQ(console.size(), before + 1);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Implementations,
    LifeStatsPlugin,
    ::testing::Values(PLUGIN_LIFE_STATS_PATH, PLUGIN_CPP_LIFE_STATS_PATH),
    [](const ::testing::TestParamInfo<const char*>& info) { return info.index == 0 ? "Rust" : "Cpp"; }
);

TEST(LifeStats, RustAndCppManifestsMatchExactly)
{
    const auto read = [](const char* path) {
        std::ifstream input(path, std::ios::binary);
        const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input), {}};
        return plugins::parse(plugins::pe_manifest(bytes));
    };
    EXPECT_EQ(read(PLUGIN_LIFE_STATS_PATH), read(PLUGIN_CPP_LIFE_STATS_PATH));
}

TEST_P(LifeStatsPlugin, PreservesEngineNamesAndSanitizesFormattingWithoutSplittingUtf8)
{
    SetOutput(1);
    g_Names[2] = std::string("Alice\n%s1\x03") + "ё";
    g_Names[3] = std::string(47, 'x') + "абв";
    nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"ak47","headshot":true,"time":1})");
    nc_runtime_event("player.health", R"({"health":0,"time":2})");
    nc_runtime_event("player.death", R"({"killer":3,"victim":1,"weapon":"deagle","headshot":false,"time":2})");
    nc_runtime_event("player.damage", R"({"health":100,"armor":0,"bits":2,"time":2})");
    alive = false;
    Frame(200);
    ASSERT_EQ(console.size(), 3u);
    ASSERT_EQ(chat.size(), 3u);
    EXPECT_EQ(chat[1], "\x04[Life Stats]\x01 Killed by " + std::string(47, 'x') + " (deagle).");
    EXPECT_EQ(chat[2], "\x04[Life Stats]\x01 Killed Alice  s1 " + std::string("ё") + " (ak47, headshot).");
    EXPECT_EQ(console[1], "\x04[Life Stats]\x01 Killed Alice  s1 " + std::string("ё") + " (ak47, headshot) (-00:01.000)\n");
}

TEST_P(LifeStatsPlugin, ReplacingOneImplementationWithTheOtherPreservesOutputChoice)
{
    SetOutput(1);
    nc_runtime_stop();
    const char* other = std::string_view(GetParam()) == PLUGIN_LIFE_STATS_PATH ? PLUGIN_CPP_LIFE_STATS_PATH : PLUGIN_LIFE_STATS_PATH;
    std::filesystem::copy_file(other, dir / L"plugins/life_stats.dll", std::filesystem::copy_options::overwrite_existing);
    nc_runtime_start(dir.c_str(), 0);
    auto rows = plugins::parse(nc_runtime_catalog()).at("plugins");
    EXPECT_FALSE(rows.at(0).at("running").get_boolean());
    rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    Restart();
    EXPECT_EQ(plugins::parse(nc_runtime_ui()).at(0).at("controls").at(0).at("value"), 1);
    nc_runtime_event("round.end", "{}");
    Frame(100);
    ASSERT_EQ(console.size(), 1u);
    ASSERT_EQ(chat.size(), 1u);
    EXPECT_EQ(network_calls, 0);
}

TEST_P(LifeStatsPlugin, RustAndCppReportsAndDeliveryTimingMatchForTheSameReplay)
{
    const auto replay = [this]() {
        console.clear();
        chat.clear();
        g_ChatCalls = 0;
        current_session.time = 0;
        current_session.frame_time = 0.01f;
        current_health = 100;
        alive = true;
        accept_chat = true;
        nc_runtime_event("hud.init", "{}");
        nc_runtime_event("round.start", "{}");
        SetOutput(1);
        Frame();
        std::vector<std::array<size_t, 3>> delivery;
        const auto frames = [this, &delivery](int count) {
            for (int i = 0; i < count; ++i)
            {
                Frame();
                delivery.push_back({console.size(), chat.size(), static_cast<size_t>(g_ChatCalls)});
            }
        };
        nc_runtime_event("player.health", R"({"health":20,"time":0.1})");
        nc_runtime_event("player.damage", R"({"health":80,"armor":5,"bits":64,"time":0.1})");
        nc_runtime_event("player.health", R"({"health":50,"time":0.2})");
        nc_runtime_event("hud.reset", "{}");
        nc_runtime_event("player.health", R"({"health":50,"time":0.2})");
        nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"awp","headshot":true,"time":0.3,"assister":3,
            "kill_details":{"killer_blind":true,"noscope":true,"penetrated":true,"through_smoke":true,"in_air":true,
            "domination_began":true,"domination":true,"revenge":true}})");
        nc_runtime_event("player.death", R"({"killer":3,"victim":2,"weapon":"ak47","headshot":false,"time":0.4,"assister":1,
            "kill_details":{"assisted_flash":true}})");
        nc_runtime_event("player.health", R"({"health":0,"time":0.5})");
        nc_runtime_event("player.death", R"({"killer":2,"victim":1,"weapon":"deagle","headshot":false,"time":0.5})");
        nc_runtime_event("player.damage", R"({"health":95,"armor":10,"bits":2,"time":0.5})");
        alive = false;
        accept_chat = false;
        frames(90);
        accept_chat = true;
        frames(240);
        nc_runtime_event("round.end", "{}");
        frames(30);
        alive = true;
        current_health = 100;
        nc_runtime_event("round.start", "{}");
        nc_runtime_event("hud.reset", "{}");
        nc_runtime_event("player.health", R"({"health":100})");
        frames(1);
        for (int i = 0; i < 520; ++i)
        {
            nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"ak47","headshot":false,"time":4})");
        }
        for (int i = 0; i < 4200; ++i)
        {
            nc_runtime_event("player.damage", R"({"health":0,"armor":1,"bits":32,"time":4})");
            if (i % 64 == 0)
            {
                frames(1);
            }
        }
        nc_runtime_event("round.end", R"({"time":5.0005})");
        frames(100);
        SetOutput(0);
        frames(100);
        nc_runtime_event("connection.changed", R"({"connected":false})");
        nc_runtime_event("map.changed", "{}");
        frames(1);
        nc_runtime_event("player.damage", R"({"health":10,"armor":0,"bits":16384,"time":10})");
        nc_runtime_event("round.end", R"({"time":11})");
        frames(30);
        return std::tuple{console, chat, delivery};
    };
    const auto first = replay();
    nc_runtime_stop();
    const char* other = std::string_view(GetParam()) == PLUGIN_LIFE_STATS_PATH ? PLUGIN_CPP_LIFE_STATS_PATH : PLUGIN_LIFE_STATS_PATH;
    std::filesystem::copy_file(other, dir / L"plugins/life_stats.dll", std::filesystem::copy_options::overwrite_existing);
    nc_runtime_start(dir.c_str(), 0);
    auto rows = plugins::parse(nc_runtime_catalog()).at("plugins");
    rows.at(0)["enabled"] = rows.at(0)["consent"] = true;
    ASSERT_STREQ(nc_runtime_save(tao::json::to_string(rows).c_str()), "");
    Restart();
    EXPECT_EQ(first, replay());
    EXPECT_EQ(network_calls, 0);
}

TEST_P(LifeStatsPlugin, InvalidEngineTextIsRejectedWithoutLossySubstitution)
{
    SetOutput(1);
    g_Names[2] = std::string("Alice") + static_cast<char>(0xff);
    nc_runtime_event("player.death", R"({"killer":1,"victim":2,"weapon":"ak47","headshot":false,"time":1})");
    nc_runtime_event("round.end", R"({"time":2})");
    Frame(200);
    ASSERT_EQ(console.size(), 1u);
    ASSERT_EQ(chat.size(), 1u);
    EXPECT_EQ(g_ChatCalls, 1);
    EXPECT_NE(console[0].find("Kills: 1."), std::string::npos);
    EXPECT_TRUE(plugins::parse(nc_runtime_catalog()).at("plugins").at(0).at("running").get_boolean());
}
