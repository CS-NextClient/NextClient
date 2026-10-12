#include <gtest/gtest.h>

#include <crosshair/crosshair.h>

TEST(CrosshairSize, ResolvesNamesInAnyCase)
{
    EXPECT_EQ(crosshair::SizeIndex("auto"), crosshair::kSizeAuto);
    EXPECT_EQ(crosshair::SizeIndex("small"), 1);
    EXPECT_EQ(crosshair::SizeIndex("MEDIUM"), 2);
    EXPECT_EQ(crosshair::SizeIndex("Large"), 3);
    EXPECT_EQ(crosshair::SizeIndex("extra_small"), 4);
}

TEST(CrosshairSize, ResolvesNumbersTheWayTheStockClientDoes)
{
    EXPECT_EQ(crosshair::SizeIndex("0"), crosshair::kSizeAuto);
    EXPECT_EQ(crosshair::SizeIndex("1"), 1);
    EXPECT_EQ(crosshair::SizeIndex("3"), 3);
    EXPECT_EQ(crosshair::SizeIndex("4"), 4);
}

TEST(CrosshairSize, FallsBackToAutoForAnythingElse)
{
    EXPECT_EQ(crosshair::SizeIndex(nullptr), crosshair::kSizeAuto);
    EXPECT_EQ(crosshair::SizeIndex(""), crosshair::kSizeAuto);
    EXPECT_EQ(crosshair::SizeIndex("huge"), crosshair::kSizeAuto);
    EXPECT_EQ(crosshair::SizeIndex("7"), crosshair::kSizeAuto);
    EXPECT_EQ(crosshair::SizeIndex("smaller"), crosshair::kSizeAuto);
}

TEST(CrosshairSize, AutoPicksTheBaseFromTheScreenWidth)
{
    EXPECT_EQ(crosshair::ScaleBase(crosshair::kSizeAuto, 1920), 640);
    EXPECT_EQ(crosshair::ScaleBase(crosshair::kSizeAuto, 1024), 640);
    EXPECT_EQ(crosshair::ScaleBase(crosshair::kSizeAuto, 800), 800);
    EXPECT_EQ(crosshair::ScaleBase(crosshair::kSizeAuto, 640), 1024);
}

TEST(CrosshairSize, NamedSizesKeepTheirBaseWhateverTheScreen)
{
    EXPECT_EQ(crosshair::ScaleBase(1, 640), 1024);
    EXPECT_EQ(crosshair::ScaleBase(2, 1920), 800);
    EXPECT_EQ(crosshair::ScaleBase(3, 320), 640);
    EXPECT_EQ(crosshair::ScaleBase(4, 1920), 1400);
}

TEST(CrosshairSpread, DecaysFasterWhileWide)
{
    EXPECT_FLOAT_EQ(crosshair::Decay(10.0f, 0.1f), 7.7f);
    EXPECT_FLOAT_EQ(crosshair::Decay(4.0f, 0.1f), 2.48f);
}

TEST(CrosshairSpread, BarGrowsWithTheOpenedGapAndScalesToTheScreen)
{
    EXPECT_EQ(crosshair::BarSize(4.0f, crosshair::kRifleGap), 5);
    EXPECT_EQ(crosshair::BarSize(15.0f, crosshair::kRifleGap), 10);

    EXPECT_FLOAT_EQ(crosshair::ScaledDistance(10.0f, 1920, 640), 30.0f);
    EXPECT_EQ(crosshair::ScaledBarSize(5, 1920, 640), 15);
    EXPECT_EQ(crosshair::ScaledBarSize(5, 640, 640), 5);
}
