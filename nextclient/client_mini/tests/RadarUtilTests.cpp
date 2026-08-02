#include <gtest/gtest.h>

#include <string>

#include "RadarUtil.h"

namespace
{
    std::string ExtractMapName(const char* level)
    {
        char out[64];
        RadarUtil_ExtractMapName(level, out);
        return out;
    }
} // namespace

TEST(RadarUtilExtractMapName, StripsPathAndExtension)
{
    EXPECT_EQ(ExtractMapName("maps/de_dust2.bsp"), "de_dust2");
    EXPECT_EQ(ExtractMapName("de_dust2.bsp"), "de_dust2");
    EXPECT_EQ(ExtractMapName("maps/cs_office.bsp"), "cs_office");
    EXPECT_EQ(ExtractMapName("maps/sub/dir/foo.bsp"), "foo");
    EXPECT_EQ(ExtractMapName("noext"), "noext");
    EXPECT_EQ(ExtractMapName(""), "");
}

TEST(RadarUtilExtractMapName, RespectsOutputSize)
{
    char out[4];
    RadarUtil_ExtractMapName("maps/abcdef.bsp", out);
    EXPECT_STREQ(out, "abc");
}

TEST(RadarUtilSiteLetter, UnderscoreSuffix)
{
    EXPECT_EQ(RadarUtil_DetectSiteLetter("tgt_a"), 'A');
    EXPECT_EQ(RadarUtil_DetectSiteLetter("bomb_B"), 'B');
}

TEST(RadarUtilSiteLetter, UnderscorePrefix)
{
    EXPECT_EQ(RadarUtil_DetectSiteLetter("a_crates"), 'A');
    EXPECT_EQ(RadarUtil_DetectSiteLetter("b_boxes"), 'B');
}

TEST(RadarUtilSiteLetter, TrailingLetter)
{
    EXPECT_EQ(RadarUtil_DetectSiteLetter("targeta"), 'A');
    EXPECT_EQ(RadarUtil_DetectSiteLetter("cratesb"), 'B');
}

TEST(RadarUtilSiteLetter, WholeName)
{
    EXPECT_EQ(RadarUtil_DetectSiteLetter("a"), 'A');
    EXPECT_EQ(RadarUtil_DetectSiteLetter("B"), 'B');
}

TEST(RadarUtilSiteLetter, NoneWhenAbsent)
{
    EXPECT_EQ(RadarUtil_DetectSiteLetter(""), 0);
    EXPECT_EQ(RadarUtil_DetectSiteLetter("xyz"), 0);
    EXPECT_EQ(RadarUtil_DetectSiteLetter("bombsite"), 0); // ends in 'e'
    EXPECT_EQ(RadarUtil_DetectSiteLetter("c"), 0);
}
