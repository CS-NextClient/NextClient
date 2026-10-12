#include <string>

#include <gtest/gtest.h>

#include "common/utf8.h"
#include "service/matchmaking/master/HttpMasterResponse.h"

namespace
{
    netadr_t Address(const char* text)
    {
        return netadr_t(text);
    }

    // Entries of a body that has to parse as a server list; records a failure when it does not
    std::vector<MasterServerEntry> ParseList(std::string_view body)
    {
        std::optional<std::vector<MasterServerEntry>> entries = HttpMasterResponse_Parse(body);
        EXPECT_TRUE(entries.has_value()) << body;

        return entries.value_or(std::vector<MasterServerEntry>{});
    }

    bool IsServerList(std::string_view body)
    {
        return HttpMasterResponse_Parse(body).has_value();
    }
} // namespace

TEST(HttpMasterResponseTest, LegacyLinesGiveAddressesWithoutModeOrCountry)
{
    std::vector<MasterServerEntry> entries = ParseList("1.2.3.4:27015\r\n5.6.7.8:27016\n");

    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].address, Address("1.2.3.4:27015"));
    EXPECT_TRUE(entries[0].details.game_mode.empty());
    EXPECT_TRUE(entries[0].details.country_code.empty());
    EXPECT_EQ(entries[1].address, Address("5.6.7.8:27016"));
}

TEST(HttpMasterResponseTest, LegacyBlankLinesAndSpacesAreSkipped)
{
    std::vector<MasterServerEntry> entries = ParseList("\n 1.2.3.4:27015\t\n \t\n\n");

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].address, Address("1.2.3.4:27015"));
}

TEST(HttpMasterResponseTest, EmptyBodyListsNothing)
{
    EXPECT_TRUE(ParseList("").empty());
    EXPECT_TRUE(ParseList("\r\n").empty());
    EXPECT_TRUE(ParseList(" \t").empty());
}

TEST(HttpMasterResponseTest, LegacyZeroAddressLineListsNothing)
{
    EXPECT_TRUE(ParseList("0.0.0.0:0").empty());

    std::vector<MasterServerEntry> entries = ParseList("1.2.3.4:27015\n0.0.0.0:0\n5.6.7.8:0\n");

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].address, Address("1.2.3.4:27015"));
}

TEST(HttpMasterResponseTest, LegacyLineThatIsNotAnAddressMakesTheBodyUnreadable)
{
    EXPECT_FALSE(IsServerList("<html>\n502 Bad Gateway\n</html>"));
    EXPECT_FALSE(IsServerList("Bad Gateway"));
    EXPECT_FALSE(IsServerList("1.2.3.4:27015\nexample.com:27015"));
    EXPECT_FALSE(IsServerList("1.2.3.4"));
    EXPECT_FALSE(IsServerList("1.2.3.4:70000"));
    EXPECT_FALSE(IsServerList("1.2.3.256:27015"));
    EXPECT_FALSE(IsServerList("-1.2.3.4:27015"));
    EXPECT_FALSE(IsServerList("1.2.3.4:27015x"));
    EXPECT_FALSE(IsServerList(R"({"address": "1.2.3.4:27015"})"));
}

TEST(HttpMasterResponseTest, ExtendedJsonGivesAddressesModesAndCountries)
{
    std::vector<MasterServerEntry> entries =
        ParseList(R"([{"address": "1.2.3.4:27015", "country": "RU", "game_mode": "Public"}, {"address": "5.6.7.8:27016"}])");

    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].address, Address("1.2.3.4:27015"));
    EXPECT_EQ(entries[0].details.game_mode, "Public");
    EXPECT_EQ(entries[0].details.country_code, "RU");
    EXPECT_EQ(entries[1].address, Address("5.6.7.8:27016"));
    EXPECT_TRUE(entries[1].details.game_mode.empty());
    EXPECT_TRUE(entries[1].details.country_code.empty());
}

TEST(HttpMasterResponseTest, ExtendedJsonIsRecognizedAfterLeadingWhitespace)
{
    std::vector<MasterServerEntry> entries = ParseList(" \r\n\t[{\"address\": \"1.2.3.4:27015\", \"game_mode\": \"GunGame\"}]");

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].details.game_mode, "GunGame");
}

TEST(HttpMasterResponseTest, CountryIsUpperCasedAndDroppedUnlessTwoAsciiLetters)
{
    std::vector<MasterServerEntry> entries = ParseList(
        R"([{"address": "1.1.1.1:1", "country": "ua"}, {"address": "1.1.1.1:2", "country": "RUS"}, )"
        R"({"address": "1.1.1.1:3", "country": 7}, )"
        R"({"address": "1.1.1.1:4", "country": "1A"}, {"address": "1.1.1.1:5", "country": ")"
        "\xD0\xAF"
        R"("}])"
    );

    ASSERT_EQ(entries.size(), 5u);
    EXPECT_EQ(entries[0].details.country_code, "UA");
    EXPECT_TRUE(entries[1].details.country_code.empty());
    EXPECT_TRUE(entries[2].details.country_code.empty());
    EXPECT_TRUE(entries[3].details.country_code.empty());
    EXPECT_TRUE(entries[4].details.country_code.empty());
}

TEST(HttpMasterResponseTest, ModeIsKeptOnlyInIdentifierForm)
{
    std::vector<MasterServerEntry> entries = ParseList(
        R"([{"address": "1.1.1.1:1", "game_mode": "BattleRoyale"}, {"address": "1.1.1.1:2", "game_mode": "Zombie_2"}, )"
        R"({"address": "1.1.1.1:3", "game_mode": "!img:5"}, {"address": "1.1.1.1:4", "game_mode": "#GameUI_Quit"}, )"
        R"({"address": "1.1.1.1:5", "game_mode": "Zombie Escape"}, {"address": "1.1.1.1:6", "game_mode": 5}, )"
        R"({"address": "1.1.1.1:7", "game_mode": "ABCDEFGHIJKLMNOPQRSTUVWXYZabcde"}, )"
        R"({"address": "1.1.1.1:8", "game_mode": "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"}])"
    );

    ASSERT_EQ(entries.size(), 8u);
    EXPECT_EQ(entries[0].details.game_mode, "BattleRoyale");
    EXPECT_EQ(entries[1].details.game_mode, "Zombie_2");
    EXPECT_TRUE(entries[2].details.game_mode.empty());
    EXPECT_TRUE(entries[3].details.game_mode.empty());
    EXPECT_TRUE(entries[4].details.game_mode.empty());
    EXPECT_TRUE(entries[5].details.game_mode.empty());
    EXPECT_EQ(entries[6].details.game_mode, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcde");
    EXPECT_TRUE(entries[7].details.game_mode.empty());
}

TEST(HttpMasterResponseTest, JsonEntriesWithoutAValidAddressAreSkipped)
{
    std::vector<MasterServerEntry> entries = ParseList(
        R"([{"game_mode": "Public"}, 7, {"address": 5}, {"address": "1.2.3.4"}, {"address": "1.2.3.4:70000"}, )"
        R"({"address": "example.com:27015"}, {"address": "0.0.0.0:0"}, {"address": "1.2.3.4:0"}, {"address": " 1.2.3.4:27015"}, )"
        R"({"address": "9.9.9.9:27015", "game_mode": "Surf"}])"
    );

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].address, Address("9.9.9.9:27015"));
    EXPECT_EQ(entries[0].details.game_mode, "Surf");
}

TEST(HttpMasterResponseTest, EmptyArrayListsNothing)
{
    EXPECT_TRUE(ParseList("[]").empty());
}

TEST(HttpMasterResponseTest, ByteOrderMarkIsSkipped)
{
    EXPECT_EQ(ParseList(std::string(kUtf8ByteOrderMark) + R"([{"address": "1.2.3.4:27015"}])").size(), 1u);
    EXPECT_EQ(ParseList(std::string(kUtf8ByteOrderMark) + "1.2.3.4:27015\n").size(), 1u);
}

TEST(HttpMasterResponseTest, MalformedJsonIsNotAServerList)
{
    EXPECT_FALSE(IsServerList(R"([{"address": "1.2.3.4:27015")"));
    EXPECT_FALSE(IsServerList("[] x"));
    EXPECT_FALSE(IsServerList(R"([{"address": "1.2.3.4:27015", "address": "5.6.7.8:27015"}])"));
}

TEST(HttpMasterResponseTest, JsonNestedTooDeeplyIsNotAServerList)
{
    EXPECT_FALSE(IsServerList(std::string(1024 * 1024, '[')));
}

TEST(HttpMasterResponseTest, NestedMembersOfAnEntryAreIgnored)
{
    std::vector<MasterServerEntry> entries = ParseList(R"([{"address": "1.2.3.4:27015", "tags": [["a"], {"b": [1]}]}])");

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].address, Address("1.2.3.4:27015"));
}
