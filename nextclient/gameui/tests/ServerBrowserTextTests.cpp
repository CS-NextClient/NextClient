#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <strtools.h>

#include "ServerBrowser/ServerBrowserText.h"

namespace
{
    constexpr const char* kShippedNativeNamesPath = NEXTCLIENT_ASSETS_DIR "/platform/servers/country_native_names.txt";

    // The Russian name of Russia, in code units: the sources are not compiled as UTF-8
    constexpr const char* kRussiaUtf8 = "\xD0\xA0\xD0\xBE\xD1\x81\xD1\x81\xD0\xB8\xD1\x8F";
    constexpr wchar_t kRussiaLowerWide[] = {0x0440, 0x043E, 0x0441, 0x0441, 0x0438, 0x044F, 0};
    constexpr wchar_t kRussiaLowerPrefixWide[] = {0x0440, 0x043E, 0x0441, 0};
    // The Ukrainian name of Ukraine in lower case, in code units
    constexpr wchar_t kUkraineLowerWide[] = {0x0443, 0x043A, 0x0440, 0x0430, 0x0457, 0x043D, 0x0430, 0};

    ServerDetailsNext Details(const char* code, const char* name)
    {
        ServerDetailsNext details;
        V_strcpy_safe(details.country_code, code);
        V_strcpy_safe(details.country_name, name);

        return details;
    }

    std::string ReadFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);

        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
} // namespace

TEST(ServerBrowserTextTest, CompareUnknownLastSortsEmptyValuesAfterKnownOnes)
{
    EXPECT_LT(ServerBrowserText_CompareUnknownLast("de", "ru"), 0);
    EXPECT_EQ(ServerBrowserText_CompareUnknownLast("DE", "de"), 0);
    EXPECT_GT(ServerBrowserText_CompareUnknownLast("", "ru"), 0);
    EXPECT_LT(ServerBrowserText_CompareUnknownLast("ru", ""), 0);
    EXPECT_EQ(ServerBrowserText_CompareUnknownLast("", ""), 0);

    EXPECT_LT(ServerBrowserText_CompareUnknownLast(L"AWP", L"csdm"), 0);
    EXPECT_GT(ServerBrowserText_CompareUnknownLast(L"", L"CSDM"), 0);
    EXPECT_LT(ServerBrowserText_CompareUnknownLast(L"CSDM", L""), 0);
}

TEST(ServerBrowserTextTest, CountryLabelIsTheNameOrTheCodeWithoutOne)
{
    EXPECT_EQ(ServerBrowserText_GetCountryLabel("DE", "Germany"), "Germany");
    EXPECT_EQ(ServerBrowserText_GetCountryLabel("DE", ""), "DE");
}

TEST(ServerBrowserTextTest, ParseCountryNativeNamesReadsCodesAndSemicolonSeparatedNames)
{
    std::string text = std::string("\xEF\xBB\xBF// codes and names\r\n") + "DE Deutschland\r\n" + "ch  Schweiz; Suisse ;;Svizzera\r\n" +
                       "RU " + kRussiaUtf8 + "\n" + "XYZ Nowhere\n" + "1A Nowhere\n" + "GB\n";

    CountryNativeNames native_names = ServerBrowserText_ParseCountryNativeNames(text);

    EXPECT_EQ(native_names.by_code.size(), 3u);
    EXPECT_EQ(native_names.by_code.at("DE"), (std::vector<std::wstring>{L"deutschland"}));
    EXPECT_EQ(native_names.by_code.at("CH"), (std::vector<std::wstring>{L"schweiz", L"suisse", L"svizzera"}));
    EXPECT_EQ(native_names.by_code.at("RU"), (std::vector<std::wstring>{kRussiaLowerWide}));
}

TEST(ServerBrowserTextTest, ParseCountryNativeNamesTakesTabsRepeatedCodesAndAnUnterminatedLastLine)
{
    CountryNativeNames native_names =
        ServerBrowserText_ParseCountryNativeNames("DE\tDeutschland\nCH Schweiz\nch Suisse\nFR ;;\nAT \xFF\nIT Italia");

    EXPECT_EQ(native_names.by_code.at("DE"), (std::vector<std::wstring>{L"deutschland"}));
    EXPECT_EQ(native_names.by_code.at("CH"), (std::vector<std::wstring>{L"schweiz", L"suisse"}));
    EXPECT_FALSE(native_names.by_code.contains("FR"));
    EXPECT_EQ(native_names.by_code.at("IT"), (std::vector<std::wstring>{L"italia"}));
}

TEST(ServerBrowserTextTest, ShippedNativeNamesParse)
{
    CountryNativeNames native_names = ServerBrowserText_ParseCountryNativeNames(ReadFile(kShippedNativeNamesPath));

    EXPECT_GE(native_names.by_code.size(), 240u);
    EXPECT_EQ(native_names.by_code.at("DE"), (std::vector<std::wstring>{L"deutschland"}));
    EXPECT_EQ(native_names.by_code.at("UA"), (std::vector<std::wstring>{kUkraineLowerWide}));
}

TEST(ServerBrowserTextTest, FindCountryCodeMatchesWholeLabelsInLowerCase)
{
    std::map<std::string, std::string> countries{{"NE", "Niger"}, {"NG", "Nigeria"}, {"XK", ""}, {"RU", kRussiaUtf8}};

    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, {}, L"niger"), "NE");
    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, {}, L"nigeria"), "NG");
    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, {}, L"nig"), "");
    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, {}, L"xk"), "XK");
    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, {}, kRussiaLowerWide), "RU");
    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, {}, L""), "");
}

TEST(ServerBrowserTextTest, FindCountryCodeMatchesWholeNativeNamesOfListedCountries)
{
    std::map<std::string, std::string> countries{{"DE", "Germany"}};
    CountryNativeNames native_names;
    native_names.by_code["DE"] = {L"deutschland"};
    native_names.by_code["CH"] = {L"schweiz"};

    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, native_names, L"deutschland"), "DE");
    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, native_names, L"deutsch"), "");
    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, native_names, L"schweiz"), "");
}

TEST(ServerBrowserTextTest, FindCountryCodeTakesTheFirstCodeOfANativeNameSharedByTwoCountries)
{
    std::map<std::string, std::string> countries{{"CH", "Switzerland"}, {"LI", "Liechtenstein"}};
    CountryNativeNames native_names;
    native_names.by_code["CH"] = {L"alpen"};
    native_names.by_code["LI"] = {L"alpen"};

    EXPECT_EQ(ServerBrowserText_FindCountryCode(countries, native_names, L"alpen"), "CH");
}

TEST(ServerBrowserTextTest, CountryFilterWithACodeMatchesOnlyThatCountry)
{
    EXPECT_TRUE(ServerBrowserText_MatchesCountryFilter(Details("NE", "Niger"), {}, "NE", L"niger"));
    EXPECT_FALSE(ServerBrowserText_MatchesCountryFilter(Details("NG", "Nigeria"), {}, "NE", L"niger"));
    EXPECT_FALSE(ServerBrowserText_MatchesCountryFilter(Details("", ""), {}, "NE", L"niger"));
}

TEST(ServerBrowserTextTest, CountryFilterTextMatchesTheCodeOrANamePrefix)
{
    EXPECT_TRUE(ServerBrowserText_MatchesCountryFilter(Details("NG", "Nigeria"), {}, "", L"nig"));
    EXPECT_TRUE(ServerBrowserText_MatchesCountryFilter(Details("NG", "Nigeria"), {}, "", L"ng"));
    EXPECT_TRUE(ServerBrowserText_MatchesCountryFilter(Details("RU", kRussiaUtf8), {}, "", kRussiaLowerPrefixWide));
    EXPECT_FALSE(ServerBrowserText_MatchesCountryFilter(Details("NG", "Nigeria"), {}, "", L"eria"));
    EXPECT_FALSE(ServerBrowserText_MatchesCountryFilter(Details("", ""), {}, "", L"ng"));
}

TEST(ServerBrowserTextTest, CountryFilterTextMatchesANativeNamePrefix)
{
    CountryNativeNames native_names;
    native_names.by_code["DE"] = {L"deutschland"};

    EXPECT_TRUE(ServerBrowserText_MatchesCountryFilter(Details("DE", "Germany"), native_names, "", L"deutsch"));
    EXPECT_FALSE(ServerBrowserText_MatchesCountryFilter(Details("DE", "Germany"), native_names, "", L"schweiz"));
    EXPECT_FALSE(ServerBrowserText_MatchesCountryFilter(Details("AT", "Austria"), native_names, "", L"deutsch"));
}
