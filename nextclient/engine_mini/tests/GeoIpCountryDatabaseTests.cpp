#include <string_view>

#include <gtest/gtest.h>

#include "service/geoip/GeoIpCountryDatabase.h"
#include "service/geoip/GeoIpLanguage.h"

namespace
{
    constexpr const char* kDatabasePath = NEXTCLIENT_ASSETS_DIR "/platform/servers/geoip_country.mmdb";
    constexpr const char* kMissingDatabasePath = NEXTCLIENT_ASSETS_DIR "/platform/servers/missing.mmdb";
    constexpr const char* kNoDatabasePath = NEXTCLIENT_ASSETS_DIR "/platform/servers/game_mode_rules.json";
    constexpr uint32_t kGooglePublicDnsIp = 0x08080808;
    constexpr uint32_t kLoopbackIp = 0x7F000001;
} // namespace

TEST(GeoIpCountryDatabaseTest, ClosedDatabaseResolvesNothing)
{
    GeoIpCountryDatabase database;
    GeoIpCountry country;
    country.code[0] = 'X';

    EXPECT_FALSE(database.is_open());
    EXPECT_FALSE(database.ResolveCountry(kGooglePublicDnsIp, "en", country));
    EXPECT_EQ(country.code[0], '\0');
}

TEST(GeoIpCountryDatabaseTest, MissingFileOrOneOfAnotherFormatDoesNotOpen)
{
    GeoIpCountryDatabase database;

    EXPECT_FALSE(database.Open(kMissingDatabasePath));
    EXPECT_FALSE(database.is_open());

    EXPECT_FALSE(database.Open(kNoDatabasePath));
    EXPECT_FALSE(database.is_open());
}

TEST(GeoIpCountryDatabaseTest, ResolvesCodeAndNameInRequestedLanguage)
{
    GeoIpCountryDatabase database;
    ASSERT_TRUE(database.Open(kDatabasePath));

    GeoIpCountry english;
    ASSERT_TRUE(database.ResolveCountry(kGooglePublicDnsIp, "en", english));
    EXPECT_STREQ(english.code, "US");
    EXPECT_STREQ(english.name, "United States");

    GeoIpCountry russian;
    ASSERT_TRUE(database.ResolveCountry(kGooglePublicDnsIp, "ru", russian));
    EXPECT_STREQ(russian.code, "US");
    EXPECT_NE(russian.name[0], '\0');
    EXPECT_STRNE(russian.name, english.name);
}

TEST(GeoIpCountryDatabaseTest, NameFallsBackToEnglishForLanguageTheDatabaseLacks)
{
    GeoIpCountryDatabase database;
    ASSERT_TRUE(database.Open(kDatabasePath));

    GeoIpCountry country;
    ASSERT_TRUE(database.ResolveCountry(kGooglePublicDnsIp, "xx", country));
    EXPECT_STREQ(country.name, "United States");
}

TEST(GeoIpCountryDatabaseTest, AddressWithoutCountryResolvesNothing)
{
    GeoIpCountryDatabase database;
    ASSERT_TRUE(database.Open(kDatabasePath));

    GeoIpCountry country;
    EXPECT_FALSE(database.ResolveCountry(kLoopbackIp, "en", country));
    EXPECT_EQ(country.code[0], '\0');
}

TEST(GeoIpCountryDatabaseTest, Utf8PrefixKeepsWholeCharacters)
{
    // "a", U+0416 in two bytes, U+20AC in three
    std::string_view text = "a\xD0\x96\xE2\x82\xAC";

    EXPECT_EQ(GeoIp_GetUtf8PrefixSize(text, 10), 6u);
    EXPECT_EQ(GeoIp_GetUtf8PrefixSize(text, 6), 6u);
    EXPECT_EQ(GeoIp_GetUtf8PrefixSize(text, 5), 3u);
    EXPECT_EQ(GeoIp_GetUtf8PrefixSize(text, 4), 3u);
    EXPECT_EQ(GeoIp_GetUtf8PrefixSize(text, 2), 1u);
    EXPECT_EQ(GeoIp_GetUtf8PrefixSize(text, 0), 0u);
}

TEST(GeoIpLanguageTest, SteamLanguagesMapToDatabaseNamesKeys)
{
    EXPECT_STREQ(GeoIp_GetNamesLanguage("russian"), "ru");
    EXPECT_STREQ(GeoIp_GetNamesLanguage("Russian"), "ru");
    EXPECT_STREQ(GeoIp_GetNamesLanguage("brazilian"), "pt-BR");
    EXPECT_STREQ(GeoIp_GetNamesLanguage("portuguese"), "pt-BR");
    EXPECT_STREQ(GeoIp_GetNamesLanguage("tchinese"), "zh-CN");
    EXPECT_STREQ(GeoIp_GetNamesLanguage("ukrainian"), "en");
    EXPECT_STREQ(GeoIp_GetNamesLanguage("english"), "en");
}
