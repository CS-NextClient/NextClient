#include <gtest/gtest.h>
#include "plugin_game_state.h"
#include <limits>

namespace
{
    using Json = tao::json::value;
    struct Packet
    {
        std::vector<unsigned char> bytes;
        Packet& byte(int v)
        {
            bytes.push_back(static_cast<unsigned char>(v));
            return *this;
        }
        Packet& word(int v)
        {
            return byte(v).byte(v >> 8);
        }
        Packet& integer(int v)
        {
            return word(v).word(v >> 16);
        }
        Packet& text(const std::string& v)
        {
            bytes.insert(bytes.end(), v.begin(), v.end());
            return byte(0);
        }
        auto send(PluginGameState& state, const char* name, float time = 10) const
        {
            return state.Message(name, bytes.data(), bytes.size(), time);
        }
    };
} // namespace
TEST(GameState, ScoreboardAndSpectatorHealthFollowWireOrder)
{
    PluginGameState state;
    auto events = Packet{}.byte(87).byte(2).send(state, "SpecHealth2");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].name, "player.health");
    EXPECT_EQ(events[0].data.at("player"), 2);
    EXPECT_EQ(state.Player(2).at("reported_health"), 87);
    Packet{}.byte(2).word(-3).word(4).word(0).word(2).send(state, "ScoreInfo");
    Packet{}.byte(2).text("CT").send(state, "TeamInfo");
    Packet{}.byte(2).byte(6).send(state, "ScoreAttrib");
    const auto p = state.Player(2);
    EXPECT_EQ(p.at("frags"), -3);
    EXPECT_EQ(p.at("deaths"), 4);
    EXPECT_EQ(p.at("team"), "CT");
    EXPECT_EQ(p.at("team_id"), 2);
    EXPECT_EQ(p.at("has_c4"), true);
    EXPECT_EQ(p.at("vip"), true);
    EXPECT_EQ(p.at("dead"), false);
    Packet{}.byte(1).byte(2).byte(1).text("ak47").send(state, "DeathMsg");
    EXPECT_EQ(state.Player(2).at("dead"), true);
    state.RemovePlayer(2);
    EXPECT_TRUE(state.Player(2).get_object().empty());
}
TEST(GameState, MalformedPacketsDoNotPartiallyOverwriteSnapshots)
{
    PluginGameState state;
    Packet{}.byte(2).word(9).word(1).word(0).word(2).send(state, "ScoreInfo");
    const auto before = state.Player(2);
    EXPECT_TRUE(Packet{}.byte(2).word(0).send(state, "ScoreInfo").empty());
    EXPECT_TRUE(Packet{}.byte(33).word(0).word(0).word(0).word(0).send(state, "ScoreInfo").empty());
    EXPECT_TRUE(Packet{}.byte(2).text(std::string(1, '\xff')).send(state, "TeamInfo").empty());
    EXPECT_EQ(state.Player(2), before);
    EXPECT_TRUE(state.Message("Health", nullptr, 1, 0).empty());
    EXPECT_TRUE(Packet{}.byte(100).send(state, "Health", std::numeric_limits<float>::quiet_NaN()).empty());
    EXPECT_TRUE(Packet{}.send(state, "unknown").empty());
}
TEST(GameState, WeaponDefinitionsSurviveHudInitializationButNotMapReset)
{
    PluginGameState state;
    Packet{}.text("weapon_ak47").byte(2).byte(90).byte(255).byte(255).byte(0).byte(1).byte(28).byte(0).send(state, "WeaponList");
    Packet{}.send(state, "InitHUD");
    const auto weapon = state.Weapon(28);
    ASSERT_TRUE(weapon.is_object());
    EXPECT_EQ(weapon.at("ammo_type"), 2);
    EXPECT_EQ(weapon.at("ammo2_type"), -1);
    EXPECT_TRUE(state.Ammo().at(2).is_null());
    Packet{}.byte(2).byte(90).send(state, "AmmoX");
    Packet{}.byte(2).byte(30).send(state, "AmmoPickup");
    EXPECT_EQ(state.Ammo().at(2), 90); // A pickup is a delta notification, not inventory.
    state.Reset();
    EXPECT_TRUE(state.Weapon(28).is_null());
    EXPECT_TRUE(state.Ammo().at(2).is_null());
}
TEST(GameState, ChatIsPermissionClassifiedAndNeverIncludedInSafeSnapshots)
{
    PluginGameState state;
    auto events = Packet{}.byte(2).text("#Cstrike_Chat_CT").text("Alex").text("private text").send(state, "SayText");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].name, "chat.message");
    EXPECT_EQ(events[0].data.at("text"), "private text");
    EXPECT_EQ(events[0].data.at("team"), true);
    EXPECT_TRUE(state.Match().get_object().empty());
    EXPECT_TRUE(state.Player(2).get_object().empty());
    events = Packet{}.byte(3).text("arbitrary server chat").send(state, "TextMsg");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].name, "chat.message");
    EXPECT_TRUE(Packet{}.byte(2).text("console text").send(state, "TextMsg").empty());
    events = Packet{}.byte(4).text("#CTs_Win").text("must not leak").send(state, "TextMsg");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].name, "round.end");
    EXPECT_EQ(events[0].data.find("arguments"), nullptr);
    EXPECT_EQ(tao::json::to_string(state.Match()).find("must not leak"), std::string::npos);
}
TEST(GameState, TimersBombAndHudStateUpdateAndReset)
{
    PluginGameState state;
    Packet{}.word(65).send(state, "Battery");
    Packet{}.byte(1).send(state, "ArmorType");
    EXPECT_EQ(state.Match().at("player.armor").at("armor"), 65);
    EXPECT_EQ(state.Match().at("player.armor").at("armor_type"), 1);
    Packet{}.word(120).send(state, "RoundTime", 20);
    EXPECT_EQ(state.Match().at("round.time").at("received_at"), 20);
    Packet{}.word(8).word(-16).word(24).byte(1).send(state, "BombDrop");
    EXPECT_EQ(state.Match().at("bomb.dropped").at("position"), Json::array({1.0, -2.0, 3.0}));
    Packet{}.send(state, "BombPickup");
    EXPECT_EQ(state.Match().find("bomb.dropped"), nullptr);
    Packet{}.byte(1).byte(75).send(state, "Flashlight");
    Packet{}.byte(74).send(state, "FlashBat");
    EXPECT_EQ(state.Match().at("hud.flashlight").at("enabled"), true);
    EXPECT_EQ(state.Match().at("hud.flashlight").at("battery"), 74);
    Packet{}.word(5).word(50).send(state, "BarTime2");
    auto events = Packet{}.send(state, "ResetHUD");
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(state.Match().find("hud.progress"), nullptr);
    Packet{}.byte(0).byte(0).send(state, "HLTV");
    EXPECT_EQ(state.Match().find("round.time"), nullptr);
    EXPECT_EQ(state.Match().find("bomb.picked_up"), nullptr);
}
TEST(GameState, EveryTruncatedPrefixIsIgnored)
{
    const auto packet = Packet{}.byte(1).byte(2).integer(8).word(8).word(16).word(24);
    for (size_t i = 0; i < packet.bytes.size(); ++i)
    {
        PluginGameState state;
        EXPECT_TRUE(state.Message("Damage", packet.bytes.data(), i, 0).empty()) << i;
        EXPECT_TRUE(state.Match().get_object().empty());
    }
}

TEST(GameState, PlayerArmorUsesReceivedMessagesAndClearsOnReconnect)
{
    PluginGameState state;
    EXPECT_EQ(state.Armor(), 0);
    Packet{}.byte(1).send(state, "ArmorType");
    EXPECT_EQ(state.Armor(), 0);
    Packet{}.word(100).send(state, "Battery");
    EXPECT_EQ(state.Armor(), 100);
    Packet{}.word(65).send(state, "Battery");
    Packet{}.byte(0).send(state, "ArmorType");
    EXPECT_EQ(state.Armor(), 65);
    EXPECT_TRUE(Packet{}.byte(25).send(state, "Battery").empty());
    EXPECT_EQ(state.Armor(), 65);
    Packet{}.send(state, "InitHUD");
    EXPECT_EQ(state.Armor(), 0);
    Packet{}.word(80).send(state, "Battery");
    state.Reset();
    EXPECT_EQ(state.Armor(), 0);
}

TEST(GameState, StockDeathsDoNotInventReGameDllFields)
{
    PluginGameState state;
    const auto events = Packet{}.byte(0).byte(2).byte(0).text("worldspawn").send(state, "DeathMsg");
    ASSERT_EQ(events.size(), 1u);
    const auto& data = events[0].data;
    EXPECT_EQ(data.at("killer"), 0);
    EXPECT_EQ(data.at("victim"), 2);
    EXPECT_EQ(data.at("weapon"), "worldspawn");
    for (const auto* field : {"death_flags", "death_position", "assister", "kill_flags", "kill_details"})
        EXPECT_EQ(data.find(field), nullptr) << field;
}

TEST(GameState, EveryDeathExtensionCombinationFollowsWireOrder)
{
    for (int flags = 0; flags < 8; ++flags)
    {
        SCOPED_TRACE(flags);
        PluginGameState state;
        auto packet = Packet{}.byte(1).byte(2).byte(1).text("ak47").integer(flags);
        if (flags & 1)
            packet.word(9).word(-17).word(32767);
        if (flags & 2)
            packet.byte(3);
        if (flags & 4)
            packet.integer(0x3ff);
        const auto events = packet.send(state, "DeathMsg", 12.5f);
        ASSERT_EQ(events.size(), 1u);
        const auto& data = events[0].data;
        EXPECT_EQ(data.at("death_flags"), flags);
        EXPECT_EQ(data.at("time"), 12.5);
        if (flags & 1)
            EXPECT_EQ(data.at("death_position"), Json::array({1.125, -2.125, 4095.875}));
        else
            EXPECT_EQ(data.find("death_position"), nullptr);
        if (flags & 2)
            EXPECT_EQ(data.at("assister"), 3);
        else
            EXPECT_EQ(data.find("assister"), nullptr);
        if (flags & 4)
        {
            EXPECT_EQ(data.at("kill_flags"), 0x3ff);
            const auto& details = data.at("kill_details").get_object();
            EXPECT_EQ(details.size(), 10u);
            for (const auto& [key, value] : details)
                EXPECT_EQ(value, true) << key;
        }
        else
            EXPECT_EQ(data.find("kill_details"), nullptr);
        EXPECT_EQ(state.Player(2).at("dead"), true);
        EXPECT_EQ(state.Player(2).find("death_position"), nullptr); // Event, not live position.
    }
}

TEST(GameState, DeathFlagsRetainUnknownBitsAndDistinguishZeroFromAbsent)
{
    PluginGameState state;
    const auto events = Packet{}
                            .byte(1)
                            .byte(2)
                            .byte(0)
                            .text("ak47")
                            .integer(0x406)
                            .byte(0)
                            .integer(static_cast<int>(0x80000000u))
                            .byte(99)
                            .send(state, "DeathMsg");
    ASSERT_EQ(events.size(), 1u);
    const auto& data = events[0].data;
    EXPECT_EQ(data.at("death_flags"), 0x406);
    EXPECT_EQ(data.at("assister"), 0);
    EXPECT_EQ(data.at("kill_flags"), 0x80000000u);
    EXPECT_EQ(data.find("death_position"), nullptr);
    for (const auto& [key, value] : data.at("kill_details").get_object())
        EXPECT_EQ(value, false) << key;
}

TEST(GameState, DeathRarityBitsAreIndependentAndDoNotLeakIntoStockMessages)
{
    const std::pair<int, const char*> cases[]{
        {0x001, "headshot"},
        {0x002, "killer_blind"},
        {0x004, "noscope"},
        {0x008, "penetrated"},
        {0x010, "through_smoke"},
        {0x020, "assisted_flash"},
        {0x040, "domination_began"},
        {0x080, "domination"},
        {0x100, "revenge"},
        {0x200, "in_air"}
    };
    PluginGameState state;
    for (const auto& [bit, name] : cases)
    {
        const auto events = Packet{}.byte(1).byte(2).byte(0).text("awp").integer(4).integer(bit).send(state, "DeathMsg");
        ASSERT_EQ(events.size(), 1u);
        for (const auto& [key, value] : events[0].data.at("kill_details").get_object())
            EXPECT_EQ(value, key == name) << name << ": " << key;
    }
    const auto stock = Packet{}.byte(1).byte(2).byte(1).text("ak47").send(state, "DeathMsg");
    ASSERT_EQ(stock.size(), 1u);
    EXPECT_EQ(stock[0].data.at("headshot"), true);
    EXPECT_EQ(stock[0].data.find("kill_details"), nullptr);
}

TEST(GameState, TruncatedDeathExtensionsAndInvalidPlayersDoNotChangeState)
{
    PluginGameState state;
    Packet{}.byte(2).byte(0).send(state, "ScoreAttrib");
    const auto before = state.Player(2);
    const auto base = Packet{}.byte(1).byte(2).byte(0).text("ak47");
    auto full = base;
    full.integer(7).word(8).word(16).word(24).byte(3).integer(0x3ff);
    for (size_t size = 0; size < full.bytes.size(); ++size)
    {
        if (size == base.bytes.size())
            continue; // The base alone is a valid stock message.
        EXPECT_TRUE(state.Message("DeathMsg", full.bytes.data(), size, 0).empty()) << size;
        EXPECT_EQ(state.Player(2), before);
    }
    EXPECT_TRUE(Packet{}.byte(33).byte(2).byte(0).text("ak47").send(state, "DeathMsg").empty());
    EXPECT_TRUE(Packet{}.byte(1).byte(0).byte(0).text("ak47").send(state, "DeathMsg").empty());
    EXPECT_TRUE(Packet{}.byte(1).byte(2).byte(0).text("ak47").integer(2).byte(33).send(state, "DeathMsg").empty());
    EXPECT_EQ(state.Player(2), before);
}

TEST(GameState, ExtendedScoreboardValuesAreOptionalSigned32BitAndClearWhenHidden)
{
    PluginGameState state;
    Packet{}.byte(100).send(state, "Health");
    Packet{}.integer(800).byte(0).send(state, "Money");
    auto health = Packet{}.byte(2).integer(1000).send(state, "HealthInfo");
    auto money = Packet{}.byte(2).integer(100000).send(state, "Account");
    ASSERT_EQ(health.size(), 1u);
    ASSERT_EQ(money.size(), 1u);
    EXPECT_EQ(health[0].name, "player.health");
    EXPECT_EQ(money[0].name, "player.money");
    EXPECT_EQ(health[0].data.at("reported_health"), 1000);
    EXPECT_EQ(money[0].data.at("reported_money"), 100000);
    EXPECT_EQ(health[0].data.at("player"), 2);
    EXPECT_EQ(health[0].data.find("health"), nullptr);
    EXPECT_EQ(money[0].data.find("money"), nullptr);
    EXPECT_EQ(state.Match().at("player.health").at("health"), 100);
    EXPECT_EQ(state.Match().at("player.money").at("money"), 800);
    for (const auto* message : {"HealthInfo", "Account"})
    {
        const char* field = std::string_view(message) == "HealthInfo" ? "reported_health" : "reported_money";
        const auto before = state.Player(2);
        auto packet = Packet{}.byte(2).integer(123);
        for (size_t size = 0; size < packet.bytes.size(); ++size)
            EXPECT_TRUE(state.Message(message, packet.bytes.data(), size, 0).empty());
        for (int index : {0, 33, 255})
            EXPECT_TRUE(Packet{}.byte(index).integer(100).send(state, message).empty());
        EXPECT_EQ(state.Player(2), before);
        for (int value : {-2, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()})
        {
            Packet{}.byte(2).integer(value).send(state, message);
            EXPECT_EQ(state.Player(2).at(field), value); // Only -1 is the protocol's hidden sentinel.
        }
        auto events = Packet{}.byte(2).integer(-1).send(state, message);
        ASSERT_EQ(events.size(), 1u);
        EXPECT_TRUE(events[0].data.at(field).is_null());
        EXPECT_TRUE(state.Player(2).at(field).is_null());
        Packet{}.byte(2).integer(0).send(state, message);
        EXPECT_EQ(state.Player(2).at(field), 0); // Known zero is not hidden.
    }
}

TEST(GameState, DefuserAndScoreboardExtensionsResetAcrossRoundsAndServers)
{
    PluginGameState state;
    Packet{}.byte(2).byte(0x88).send(state, "ScoreAttrib");
    EXPECT_EQ(state.Player(2).at("attribute_flags"), 0x88);
    EXPECT_EQ(state.Player(2).at("has_defuser"), true);
    Packet{}.byte(2).byte(0).send(state, "ScoreAttrib");
    EXPECT_TRUE(state.Player(2).at("has_defuser").is_null());
    for (int reset = 0; reset < 4; ++reset)
    {
        Packet{}.byte(2).byte(8).send(state, "ScoreAttrib");
        Packet{}.byte(2).integer(100).send(state, "HealthInfo");
        Packet{}.byte(2).integer(800).send(state, "Account");
        if (reset == 0)
            Packet{}.byte(0).byte(0).send(state, "HLTV");
        else if (reset == 1)
            Packet{}.send(state, "InitHUD");
        else if (reset == 2)
            state.RemovePlayer(2);
        else
            state.Reset();
        const auto player = state.Player(2);
        for (const auto* field : {"reported_health", "reported_money", "has_defuser", "attribute_flags"})
            EXPECT_EQ(player.find(field), nullptr) << reset << ": " << field;
    }
}

TEST(GameState, AllStandardRoundOutcomesProduceSafeRoundEvents)
{
    for (const auto* reason :
         {"#Terrorists_Win",
          "#CTs_Win",
          "#Round_Draw",
          "#Target_Bombed",
          "#Bomb_Defused",
          "#Target_Saved",
          "#All_Hostages_Rescued",
          "#Hostages_Not_Rescued",
          "#VIP_Escaped",
          "#VIP_Assassinated",
          "#VIP_Not_Escaped",
          "#Terrorists_Escaped",
          "#CTs_PreventEscape",
          "#Escaping_Terrorists_Neutralized",
          "#Terrorists_Not_Escaped"})
    {
        SCOPED_TRACE(reason);
        PluginGameState state;
        const auto events = Packet{}.byte(4).text(reason).text("private argument").send(state, "TextMsg");
        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].name, "round.end");
        EXPECT_EQ(events[0].data, (Json{{"reason", reason}, {"time", 10.0}}));
        EXPECT_EQ(state.Match().at("round.end"), (Json{{"reason", reason}}));
        Packet{}.byte(0).byte(0).send(state, "HLTV");
        EXPECT_EQ(state.Match().find("round.end"), nullptr);
    }
}
TEST(GameState, DisabledIconsAreRemovedInsteadOfAccumulating)
{
    PluginGameState state;
    Packet{}.byte(1).text("buyzone").byte(1).byte(2).byte(3).send(state, "StatusIcon");
    EXPECT_NE(state.Match().at("status_icons").find("buyzone"), nullptr);
    const auto events = Packet{}.byte(0).text("buyzone").send(state, "StatusIcon");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data.at("state"), 0);
    for (int i = 0; i < 5000; ++i)
        Packet{}.byte(0).text("disabled_" + std::to_string(i)).send(state, "StatusIcon");
    EXPECT_TRUE(state.Match().at("status_icons").get_object().empty());
}
TEST(GameState, NamedSnapshotsStayBoundedAndExistingEntriesRemainUpdateable)
{
    PluginGameState state;
    for (int i = 0; i < 5000; ++i)
    {
        const auto name = std::to_string(i);
        Packet{}.text(name).word(i).send(state, "TeamScore");
        Packet{}.byte(1).text(name).byte(1).byte(2).byte(3).send(state, "StatusIcon");
    }
    EXPECT_EQ(state.Match().at("team_scores").get_object().size(), 32u);
    EXPECT_EQ(state.Match().at("status_icons").get_object().size(), 64u);
    Packet{}.text("0").word(123).send(state, "TeamScore");
    Packet{}.byte(2).text("0").byte(4).byte(5).byte(6).send(state, "StatusIcon");
    EXPECT_EQ(state.Match().at("team_scores").at("0"), 123);
    EXPECT_EQ(state.Match().at("status_icons").at("0").at("state"), 2);
    Packet{}.byte(0).text("0").send(state, "StatusIcon");
    Packet{}.byte(1).text("replacement").byte(1).byte(2).byte(3).send(state, "StatusIcon");
    EXPECT_NE(state.Match().at("status_icons").find("replacement"), nullptr);
    const auto before = state.Match();
    EXPECT_TRUE(Packet{}.text("0").byte(1).send(state, "TeamScore").empty());
    EXPECT_TRUE(Packet{}.byte(1).text("replacement").byte(1).send(state, "StatusIcon").empty());
    EXPECT_TRUE(Packet{}.byte(0).text(std::string(1, '\xff')).send(state, "StatusIcon").empty());
    EXPECT_EQ(state.Match(), before);
    EXPECT_LT(tao::json::to_string(before).size(), 16384u);
    state.Reset();
    Packet{}.text(std::string(65, 'a')).word(1).send(state, "TeamScore");
    Packet{}.byte(1).text(std::string(65, 'a')).byte(1).byte(2).byte(3).send(state, "StatusIcon");
    EXPECT_TRUE(state.Match().at("team_scores").get_object().empty());
    EXPECT_TRUE(state.Match().at("status_icons").get_object().empty());
    Packet{}.text(std::string(64, 'a')).word(1).send(state, "TeamScore");
    Packet{}.byte(1).text(std::string(64, 'a')).byte(1).byte(2).byte(3).send(state, "StatusIcon");
    EXPECT_EQ(state.Match().at("team_scores").get_object().size(), 1u);
    EXPECT_EQ(state.Match().at("status_icons").get_object().size(), 1u);
    Packet{}.send(state, "InitHUD");
    EXPECT_TRUE(state.Match().at("team_scores").get_object().empty());
    EXPECT_TRUE(state.Match().at("status_icons").get_object().empty());
}
