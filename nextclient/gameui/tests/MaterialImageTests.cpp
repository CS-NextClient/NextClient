#include <gtest/gtest.h>

#include <limits>

#include <ncl_material/MaterialPreset.h>

TEST(MaterialImage, AcceptsPixelBudgetIndependentlyOfAspectRatio)
{
    EXPECT_TRUE(ncl_material::IsAdmissibleImage(2048, 512));
    EXPECT_TRUE(ncl_material::IsAdmissibleImage(512, 2048));
    EXPECT_TRUE(ncl_material::IsAdmissibleImage(1024, 1024));
    EXPECT_FALSE(ncl_material::IsAdmissibleImage(1025, 1024));
}

TEST(MaterialImage, RejectsInvalidAndOverflowingDimensions)
{
    EXPECT_FALSE(ncl_material::IsAdmissibleImage(0, 100));
    EXPECT_FALSE(ncl_material::IsAdmissibleImage(100, 0));
    EXPECT_FALSE(ncl_material::IsAdmissibleImage(-1, -1));
    EXPECT_FALSE(ncl_material::IsAdmissibleImage(1, -1));
    EXPECT_FALSE(ncl_material::IsAdmissibleImage(std::numeric_limits<int64_t>::max(), 2));
    EXPECT_FALSE(ncl_material::IsAdmissibleImage(2, std::numeric_limits<int64_t>::max()));
}

TEST(MaterialImage, AcceptsMaximumUncompressedTgaAndRejectsExcessFileBytes)
{
    uint64_t rgba_bytes = 2048ull * 512ull * 4ull;

    EXPECT_TRUE(ncl_material::IsAdmissibleImageFileSize(rgba_bytes + 18));
    EXPECT_TRUE(ncl_material::IsAdmissibleImageFileSize(ncl_material::kMaxImageFileBytes));
    EXPECT_FALSE(ncl_material::IsAdmissibleImageFileSize(uint64_t(ncl_material::kMaxImageFileBytes) + 1));
    EXPECT_FALSE(ncl_material::IsAdmissibleImageFileSize(0));
    EXPECT_FALSE(ncl_material::IsAdmissibleImageFileSize(std::numeric_limits<uint64_t>::max()));
}
