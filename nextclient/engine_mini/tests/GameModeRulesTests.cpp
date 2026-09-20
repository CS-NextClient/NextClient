#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "common/utf8.h"
#include "service/matchmaking/GameModeRules.h"

namespace
{
    constexpr const char* kShippedRulesPath = NEXTCLIENT_ASSETS_DIR "/platform/servers/game_mode_rules.json";

    // The Russian word for zombie in small and capital letters, in UTF-8 code units: the sources are not compiled as UTF-8
    constexpr const char* kZombieSmallUtf8 = "\xD0\xB7\xD0\xBE\xD0\xBC\xD0\xB1\xD0\xB8";
    constexpr const char* kZombieCapitalUtf8 = "\xD0\x97\xD0\x9E\xD0\x9C\xD0\x91\xD0\x98";
    constexpr const char* kZombieSmallCp1251 = "\xE7\xEE\xEC\xE1\xE8";

    // "publico" with U+00FA and U+00DA, "pablik" in Ukrainian with U+0456 and U+0406
    constexpr const char* kPublicoSmallUtf8 =
        "p\xC3\xBA"
        "blico";
    constexpr const char* kPublicoCapitalUtf8 =
        "P\xC3\x9A"
        "BLICO";
    constexpr const char* kPablikSmallUtf8 = "\xD0\xBF\xD0\xB0\xD0\xB1\xD0\xBB\xD1\x96\xD0\xBA";
    constexpr const char* kPablikCapitalUtf8 = "\xD0\x9F\xD0\x90\xD0\x91\xD0\x9B\xD0\x86\xD0\x9A";

    constexpr const char* kMultiplicationSignUtf8 = "\xC3\x97";

    GameModeRules ParseRules(const std::string& json)
    {
        GameModeRules rules;
        EXPECT_TRUE(rules.Parse(json));

        return rules;
    }

    std::string ReadFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
} // namespace

TEST(GameModeRulesTest, TextOfAnotherShapeLeavesNoRules)
{
    GameModeRules rules = ParseRules(R"({"rules": [{"game_mode": "Zombie", "keywords": ["zombie"]}]})");

    EXPECT_FALSE(rules.Parse("not json"));
    EXPECT_FALSE(rules.is_loaded());
    EXPECT_STREQ(rules.Detect("", "Zombie server", ""), "");

    EXPECT_FALSE(rules.Parse("[]"));
    EXPECT_FALSE(rules.Parse(R"({"rules": {}})"));
}

TEST(GameModeRulesTest, NestingDeeperThanTheRulesFormatIsMalformed)
{
    GameModeRules rules;

    EXPECT_FALSE(rules.Parse(R"({"rules": )" + std::string(100000, '[')));
    EXPECT_FALSE(rules.is_loaded());
}

TEST(GameModeRulesTest, ByteOrderMarkIsSkipped)
{
    GameModeRules rules = ParseRules(std::string(kUtf8ByteOrderMark) + R"({"rules": [{"game_mode": "Zombie", "keywords": ["zm"]}]})");

    EXPECT_STREQ(rules.Detect("", "[ZM] server", ""), "Zombie");
}

TEST(GameModeRulesTest, KeywordMatchesWholeWordsInAnyLetterCase)
{
    GameModeRules rules = ParseRules(R"({"rules": [{"game_mode": "Deathmatch", "keywords": ["dm"]}]})");

    EXPECT_STREQ(rules.Detect("", "[DM] Frag server", ""), "Deathmatch");
    EXPECT_STREQ(rules.Detect("", "Admin server", ""), "");
    EXPECT_STREQ(rules.Detect("", "DMx", ""), "");
}

TEST(GameModeRulesTest, StarLetsTheKeywordBePartOfALongerWord)
{
    GameModeRules rules = ParseRules(R"({"rules": [
        {"game_mode": "Zombie", "keywords": ["zombie*"]},
        {"game_mode": "Knife", "keywords": ["*knife"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "ZombiePlague", ""), "Zombie");
    EXPECT_STREQ(rules.Detect("", "SuperKnife arena", ""), "Knife");
    EXPECT_STREQ(rules.Detect("", "Knifes", ""), "");
}

TEST(GameModeRulesTest, KeywordOfSeveralWordsMatchesThemInSequence)
{
    GameModeRules rules = ParseRules(R"({"rules": [{"game_mode": "Deathrun", "keywords": ["death run"]}]})");

    EXPECT_STREQ(rules.Detect("", "Death-Run #1", ""), "Deathrun");
    EXPECT_STREQ(rules.Detect("", "Death of the run", ""), "");
}

TEST(GameModeRulesTest, DotBetweenWordCharactersKeepsTheWordWhole)
{
    GameModeRules rules = ParseRules(R"({"rules": [{"game_mode": "Kreedz", "keywords": ["kz"]}]})");

    EXPECT_STREQ(rules.Detect("", "JoinGame.kz hosting", ""), "");
    EXPECT_STREQ(rules.Detect("", "KZ. Jump", ""), "Kreedz");
}

TEST(GameModeRulesTest, CyrillicLettersMatchInAnyLetterCase)
{
    GameModeRules rules = ParseRules(std::string(R"({"rules": [{"game_mode": "Zombie", "keywords": [")") + kZombieSmallUtf8 + R"("]}]})");

    EXPECT_STREQ(rules.Detect("", std::string("[") + kZombieCapitalUtf8 + "] server", ""), "Zombie");
}

TEST(GameModeRulesTest, Latin1AndUkrainianCapitalsFoldToSmallLetters)
{
    GameModeRules rules = ParseRules(
        std::string(R"({"rules": [{"game_mode": "Public", "keywords": [")") + kPublicoSmallUtf8 + R"(", ")" + kPablikSmallUtf8 + R"("]}]})"
    );

    EXPECT_STREQ(rules.Detect("", std::string(kPublicoCapitalUtf8) + " #1", ""), "Public");
    EXPECT_STREQ(rules.Detect("", std::string(kPablikCapitalUtf8) + " #1", ""), "Public");
}

TEST(GameModeRulesTest, MultiplicationSignSeparatesWords)
{
    GameModeRules rules = ParseRules(R"({"rules": [{"game_mode": "Deathmatch", "keywords": ["dm"]}]})");

    EXPECT_STREQ(rules.Detect("", std::string("zm") + kMultiplicationSignUtf8 + "dm", ""), "Deathmatch");
}

TEST(GameModeRulesTest, BytesThatAreNoUtf8SeparateWords)
{
    GameModeRules rules =
        ParseRules(std::string(R"({"rules": [{"game_mode": "Zombie", "keywords": ["zm", ")") + kZombieSmallUtf8 + R"("]}]})");

    // a lead byte at the end, a lead byte before ASCII, a stray continuation byte, bytes no sequence starts with
    EXPECT_STREQ(rules.Detect("", "zm\xD0", ""), "Zombie");
    EXPECT_STREQ(rules.Detect("", "\xD0zm", ""), "Zombie");
    EXPECT_STREQ(rules.Detect("", "\x80zm\xBF", ""), "Zombie");
    EXPECT_STREQ(rules.Detect("", "\xF8zm\xFF", ""), "Zombie");

    // a surrogate encoded in three bytes and text in the Windows-1251 code page
    EXPECT_STREQ(rules.Detect("", "\xED\xA0\x80", ""), "");
    EXPECT_STREQ(rules.Detect("", kZombieSmallCp1251, ""), "");
}

TEST(GameModeRulesTest, KeywordsLookAtTheGameDescriptionAndTheServerNameOnly)
{
    GameModeRules rules = ParseRules(R"({"rules": [{"game_mode": "Zombie", "keywords": ["zombie"]}]})");

    EXPECT_STREQ(rules.Detect("Zombie Plague", "Best server", "de_dust2"), "Zombie");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Zombie server", "de_dust2"), "Zombie");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Best server", "zombie_town"), "");
}

TEST(GameModeRulesTest, RuleMatchesOnlyWhenEachOfItsListsMatches)
{
    GameModeRules rules = ParseRules(R"({"rules": [{"game_mode": "Awp", "keywords": ["awp"], "maps": ["awp"]}]})");

    EXPECT_STREQ(rules.Detect("", "AWP forever", "awp_india"), "Awp");
    EXPECT_STREQ(rules.Detect("", "AWP forever", "de_dust2"), "");
    EXPECT_STREQ(rules.Detect("", "Best server", "awp_india"), "");
}

TEST(GameModeRulesTest, RulesOfOnePriorityNamingDifferentModesLeaveTheModeUnknown)
{
    GameModeRules rules = ParseRules(R"({"rules": [
        {"game_mode": "Surf", "keywords": ["surf"]},
        {"game_mode": "Surf", "maps": ["surf"]},
        {"game_mode": "JailBreak", "keywords": ["jail"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "Surf, no jail", "surf_ski"), "");
    EXPECT_STREQ(rules.Detect("", "Surf only", "surf_ski"), "Surf");
}

TEST(GameModeRulesTest, RulesOfAHigherPriorityDecide)
{
    GameModeRules rules = ParseRules(R"({"rules": [
        {"game_mode": "Zombie", "priority": -1, "keywords": ["zombie"]},
        {"game_mode": "ZombiePlague", "keywords": ["zombie plague"]},
        {"game_mode": "Public", "priority": -2, "keywords": ["public"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "Zombie Plague public", ""), "ZombiePlague");
    EXPECT_STREQ(rules.Detect("", "Zombie public", ""), "Zombie");
    EXPECT_STREQ(rules.Detect("", "Public", ""), "Public");
}

TEST(GameModeRulesTest, RuleOfAHigherPriorityDecidesOverDisagreeingRulesBeforeIt)
{
    GameModeRules rules = ParseRules(R"({"rules": [
        {"game_mode": "Surf", "priority": -1, "keywords": ["surf"]},
        {"game_mode": "JailBreak", "priority": -1, "keywords": ["jail"]},
        {"game_mode": "Deathrun", "keywords": ["deathrun"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "Surf jail deathrun", ""), "Deathrun");
    EXPECT_STREQ(rules.Detect("", "Surf jail", ""), "");
}

TEST(GameModeRulesTest, IdentifiersDifferingInLetterCaseAgree)
{
    GameModeRules rules = ParseRules(R"({"rules": [
        {"game_mode": "Zombie", "keywords": ["zombie"]},
        {"game_mode": "zombie", "maps": ["zm"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "Zombie server", "zm_town"), "Zombie");
}

TEST(GameModeRulesTest, RuleWithoutAGameModeLeavesTheModeUnknown)
{
    GameModeRules rules = ParseRules(R"({"default_game_mode": "Public", "rules": [
        {"game_mode": "", "keywords": ["ttt"]},
        {"game_mode": "Deathrun", "keywords": ["deathrun"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "TTT server", ""), "");
    EXPECT_STREQ(rules.Detect("", "Deathrun server", ""), "Deathrun");
}

TEST(GameModeRulesTest, ServersNoRuleNamesGetTheDefaultMode)
{
    GameModeRules rules = ParseRules(R"({"default_game_mode": "Public", "rules": [
        {"game_mode": "Zombie", "keywords": ["zombie"]},
        {"game_mode": "JailBreak", "keywords": ["jail"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "Best server", "de_dust2"), "Public");
    EXPECT_STREQ(rules.Detect("", "Zombie server", ""), "Zombie");

    // rules of one priority naming different modes decide nothing, so the default takes over
    EXPECT_STREQ(rules.Detect("", "Zombie jail", ""), "Public");
}

TEST(GameModeRulesTest, DefaultOfAnotherFormLeavesTheModeUnknown)
{
    GameModeRules rules = ParseRules(R"({"default_game_mode": "Not a mode", "rules": [{"game_mode": "Zombie", "keywords": ["zombie"]}]})");

    EXPECT_STREQ(rules.Detect("", "Best server", "de_dust2"), "");
    EXPECT_STREQ(rules.Detect("", "Zombie server", ""), "Zombie");
}

TEST(GameModeRulesTest, InvalidRulesAreSkipped)
{
    GameModeRules rules = ParseRules(R"({"rules": [
        {"game_mode": "Zombie Plague", "keywords": ["zombie"]},
        {"game_mode": "Knife", "priority": "high", "keywords": ["zombie"]},
        {"game_mode": "Awp"},
        {"game_mode": "Zombie", "keywords": ["zombie"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "Zombie server", ""), "Zombie");
}

TEST(GameModeRulesTest, InvalidPrioritiesListsAndEntriesAreSkipped)
{
    GameModeRules rules = ParseRules(R"({"rules": [
        {"game_mode": "Knife", "priority": 1.5, "keywords": ["knife"]},
        {"game_mode": "Knife", "priority": 4294967296, "keywords": ["knife"]},
        {"game_mode": "Awp", "keywords": "awp"},
        {"game_mode": "Aim", "keywords": [1, "*", "aim"]}
    ]})");

    EXPECT_STREQ(rules.Detect("", "Knife arena", ""), "");
    EXPECT_STREQ(rules.Detect("", "AWP only", ""), "");
    EXPECT_STREQ(rules.Detect("", "Aim server", ""), "Aim");
}

TEST(GameModeRulesTest, ShippedRulesDetectTypicalServers)
{
    GameModeRules rules = ParseRules(ReadFile(kShippedRulesPath));

    EXPECT_STREQ(rules.Detect("Zombie Plague 4.3", "[ZM] Best server", "zm_ice_attack3"), "Zombie");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Classic Public #1", "de_dust2"), "Public");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "AWP India forever", "awp_india"), "Awp");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Best server", "awp_india"), "Public");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Surf Gateway // NO JAIL", "surf_ski_2"), "Public");
    EXPECT_STREQ(rules.Detect("CSDM 2.1", "CSDM FFA", "de_dust2"), "Deathmatch");
    EXPECT_STREQ(rules.Detect("AutoMix", "Mix 5x5", "de_dust2"), "Mix");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "JoinGame.kz hosting", "de_dust2"), "Public");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "[ZE] Escape server", "ze_jurassicpark"), "Zombie");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Serwer ze statystykami", "de_dust2"), "Public");
}

TEST(GameModeRulesTest, ShippedRulesMergeTheModesOfOneFamily)
{
    GameModeRules rules = ParseRules(ReadFile(kShippedRulesPath));

    // the zombie mods no longer disagree with each other
    EXPECT_STREQ(rules.Detect("Zombie Plague", "[ZE] Escape", "ze_castle"), "Zombie");
    EXPECT_STREQ(rules.Detect("Biohazard 2.1", "Biohazard server", "de_dust2"), "Zombie");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Only HS respawn", "de_dust2"), "Deathmatch");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Bhop pro", "bhop_monster"), "Kreedz");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Knife arena", "ka_bunker"), "Knife");
    EXPECT_STREQ(rules.Detect("War3FT 3.0", "Warcraft server", "de_dust2"), "Warcraft");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "Furien mod", "de_dust2"), "Special");
    EXPECT_STREQ(rules.Detect("Counter-Strike", "MultiMod server", "de_dust2"), "Special");
}
