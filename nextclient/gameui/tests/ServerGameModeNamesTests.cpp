#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <nitro_utils/string_utils.h>

#include "ServerBrowser/ServerGameModeNames.h"

namespace
{
    constexpr const char* kShippedRulesPath = NEXTCLIENT_ASSETS_DIR "/platform/servers/game_mode_rules.json";
    constexpr const char* kEnglishNamesPath = NEXTCLIENT_ASSETS_DIR "/cstrike/resource/nextclient_english.txt";
    constexpr std::string_view kRuleGameModeKey = "\"game_mode\": \"";

    std::string ReadFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    // The UTF-8 text of a localization file, which is UTF-16 with a byte order mark
    std::string ReadUtf16File(const char* path)
    {
        std::string bytes = ReadFile(path);
        std::wstring wide(reinterpret_cast<const wchar_t*>(bytes.data()), bytes.size() / sizeof(wchar_t));

        return nitro_utils::wide_to_utf8(wide);
    }
} // namespace

TEST(ServerGameModeNamesTest, KnownModesResolveToTheirTokensInAnyLetterCase)
{
    EXPECT_STREQ(ServerGameMode_GetNameToken("Deathmatch"), "#ServerBrowser_GameMode_Deathmatch");
    EXPECT_STREQ(ServerGameMode_GetNameToken("gungame"), "#ServerBrowser_GameMode_GunGame");
    EXPECT_EQ(ServerGameMode_GetNameToken("ZombiePlague"), nullptr);
    EXPECT_EQ(ServerGameMode_GetNameToken("BattleRoyale"), nullptr);
    EXPECT_EQ(ServerGameMode_GetNameToken(""), nullptr);
}

TEST(ServerGameModeNamesTest, FindIndexLocatesKnownModesInAnyLetterCase)
{
    EXPECT_EQ(ServerGameMode_FindIndex("Public"), 0);
    EXPECT_STREQ(kServerGameModeNames[ServerGameMode_FindIndex("gungame")].name, "GunGame");
    EXPECT_EQ(ServerGameMode_FindIndex("BattleRoyale"), -1);
    EXPECT_EQ(ServerGameMode_FindIndex(""), -1);
}

TEST(ServerGameModeNamesTest, FilterItemListsTheServersOfItsOwnModeOnly)
{
    EXPECT_TRUE(ServerGameMode_MatchesFilter("zombie", "Zombie"));
    EXPECT_FALSE(ServerGameMode_MatchesFilter("ZombiePlague", "Zombie"));
    EXPECT_FALSE(ServerGameMode_MatchesFilter("Deathmatch", "Zombie"));
    EXPECT_FALSE(ServerGameMode_MatchesFilter("BattleRoyale", "Zombie"));
}

TEST(ServerGameModeNamesTest, CellTextIsTheTokenOrTheIdentifier)
{
    EXPECT_STREQ(ServerGameMode_GetCellText("Surf"), "#ServerBrowser_GameMode_Surf");
    EXPECT_STREQ(ServerGameMode_GetCellText("BattleRoyale"), "BattleRoyale");
    EXPECT_STREQ(ServerGameMode_GetCellText(""), "");
}

TEST(ServerGameModeNamesTest, ShippedRulesNameOnlyModesOfTheTable)
{
    std::string rules = ReadFile(kShippedRulesPath);
    ASSERT_FALSE(rules.empty());

    size_t found = 0;

    for (size_t pos = rules.find(kRuleGameModeKey); pos != std::string::npos; pos = rules.find(kRuleGameModeKey, pos + 1))
    {
        size_t value = pos + kRuleGameModeKey.size();
        std::string game_mode = rules.substr(value, rules.find('"', value) - value);
        found++;

        if (game_mode.empty())
        {
            continue;
        }

        EXPECT_GE(ServerGameMode_FindIndex(game_mode.c_str()), 0) << game_mode;
    }

    EXPECT_GT(found, 0u);
}

TEST(ServerGameModeNamesTest, EveryModeHasAnEnglishDisplayName)
{
    std::string names = ReadUtf16File(kEnglishNamesPath);
    ASSERT_FALSE(names.empty());

    for (const ServerGameModeName& mode_name : kServerGameModeNames)
    {
        std::string token = std::string("\"") + (mode_name.token + 1) + "\"";

        EXPECT_NE(names.find(token), std::string::npos) << mode_name.name;
    }
}
