#include <numeric>

#include <gtest/gtest.h>
#include <strtools.h>

#include "ServerBrowser/ServerFilterCounts.h"

namespace
{
    ServerDetailsNext Details(const char* game_mode, const char* country_code)
    {
        ServerDetailsNext details;
        V_strcpy_safe(details.game_mode, game_mode);
        V_strcpy_safe(details.country_code, country_code);

        return details;
    }
} // namespace

TEST(ServerFilterCountsTest, ServerPassingBothFiltersCountsForItsModeAndCountry)
{
    ServerFilterCounts counts;
    ServerFilterCounts_Add(Details("Zombie", "RU"), true, true, &counts);
    ServerFilterCounts_Add(Details("zombie", "RU"), true, true, &counts);

    EXPECT_EQ(counts.all_game_modes, 2);
    EXPECT_EQ(counts.game_modes[ServerGameMode_FindIndex("Zombie")], 2);
    EXPECT_EQ(counts.all_countries, 2);
    EXPECT_EQ(counts.countries.at("RU"), 2);
}

TEST(ServerFilterCountsTest, ModeItemsCountOnlyServersPassingTheCountryFilter)
{
    ServerFilterCounts counts;
    ServerFilterCounts_Add(Details("Zombie", "RU"), true, false, &counts);

    EXPECT_EQ(counts.all_game_modes, 0);
    EXPECT_EQ(counts.game_modes[ServerGameMode_FindIndex("Zombie")], 0);
    EXPECT_EQ(counts.all_countries, 1);
    EXPECT_EQ(counts.countries.at("RU"), 1);
}

TEST(ServerFilterCountsTest, CountryItemsCountOnlyServersPassingTheModeFilter)
{
    ServerFilterCounts counts;
    ServerFilterCounts_Add(Details("Zombie", "RU"), false, true, &counts);

    EXPECT_EQ(counts.all_game_modes, 1);
    EXPECT_EQ(counts.game_modes[ServerGameMode_FindIndex("Zombie")], 1);
    EXPECT_EQ(counts.all_countries, 0);
    EXPECT_TRUE(counts.countries.empty());
}

TEST(ServerFilterCountsTest, ServerCountsForItsOwnModeOnly)
{
    ServerFilterCounts counts;
    ServerFilterCounts_Add(Details("Zombie", "RU"), true, true, &counts);

    EXPECT_EQ(counts.game_modes[ServerGameMode_FindIndex("Zombie")], 1);
    EXPECT_EQ(std::accumulate(counts.game_modes.begin(), counts.game_modes.end(), 0), 1);
}

TEST(ServerFilterCountsTest, UnknownModeAndMissingCountryCountOnlyForAll)
{
    ServerFilterCounts counts;
    ServerFilterCounts_Add(Details("BattleRoyale", ""), true, true, &counts);
    ServerFilterCounts_Add(Details("ZombiePlague", ""), true, true, &counts);

    EXPECT_EQ(counts.all_game_modes, 2);
    EXPECT_EQ(std::accumulate(counts.game_modes.begin(), counts.game_modes.end(), 0), 0);
    EXPECT_EQ(counts.all_countries, 2);
    EXPECT_TRUE(counts.countries.empty());
}
