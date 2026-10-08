#include <gtest/gtest.h>

#include <strtools.h>

#include "service/matchmaking/ServerCountry.h"

namespace
{
    GeoIpCountry Country(const char* code, const char* name)
    {
        GeoIpCountry country;
        V_strcpy_safe(country.code, code);
        V_strcpy_safe(country.name, name);

        return country;
    }
} // namespace

TEST(ServerCountryTest, GeoIpCountryIsUsedWhenTheMasterReportsNone)
{
    GeoIpCountry geoip = Country("DE", "Germany");
    ServerDetailsNext details;

    ServerCountry_Fill("", &geoip, {}, &details);

    EXPECT_STREQ(details.country_code, "DE");
    EXPECT_STREQ(details.country_name, "Germany");
}

TEST(ServerCountryTest, MasterCodeWinsAndTakesTheNameKnownForThatCode)
{
    GeoIpCountry geoip = Country("NL", "Netherlands");
    ServerDetailsNext details;

    ServerCountry_Fill("DE", &geoip, {{"DE", "Germany"}}, &details);

    EXPECT_STREQ(details.country_code, "DE");
    EXPECT_STREQ(details.country_name, "Germany");
}

TEST(ServerCountryTest, MasterCodeWithoutAKnownNameLeavesTheNameEmpty)
{
    GeoIpCountry geoip = Country("NL", "Netherlands");
    ServerDetailsNext details;

    ServerCountry_Fill("DE", &geoip, {}, &details);

    EXPECT_STREQ(details.country_code, "DE");
    EXPECT_STREQ(details.country_name, "");
}

TEST(ServerCountryTest, MasterCodeMatchingTheGeoIpRecordTakesTheRecordName)
{
    GeoIpCountry geoip = Country("DE", "Deutschland");
    ServerDetailsNext details;

    ServerCountry_Fill("DE", &geoip, {{"DE", "Germany"}}, &details);

    EXPECT_STREQ(details.country_code, "DE");
    EXPECT_STREQ(details.country_name, "Deutschland");
}

TEST(ServerCountryTest, MasterCodeWithoutAGeoIpRecordTakesTheNameKnownForThatCode)
{
    ServerDetailsNext details;

    ServerCountry_Fill("UA", nullptr, {{"UA", "Ukraine"}}, &details);

    EXPECT_STREQ(details.country_code, "UA");
    EXPECT_STREQ(details.country_name, "Ukraine");
}

TEST(ServerCountryTest, UnknownCountryLeavesTheFieldsEmpty)
{
    ServerDetailsNext details;

    ServerCountry_Fill("", nullptr, {{"UA", "Ukraine"}}, &details);

    EXPECT_STREQ(details.country_code, "");
    EXPECT_STREQ(details.country_name, "");
}
