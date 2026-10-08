#include <gtest/gtest.h>

#include <view/view_fov.h>

TEST(ScreenFov, LeavesTheFieldAloneWithoutHorPlus)
{
    EXPECT_FLOAT_EQ(view_fov::ScreenFov(90.0f, 16.0f / 9.0f, false), 90.0f);
}

TEST(ScreenFov, LeavesAFourByThreeScreenAlone)
{
    EXPECT_FLOAT_EQ(view_fov::ScreenFov(90.0f, 0.75f, true), 90.0f);
}

TEST(ScreenFov, WidensAWideScreenToKeepTheVerticalField)
{
    EXPECT_NEAR(view_fov::ScreenFov(90.0f, 16.0f / 9.0f, true), 106.26f, 0.01f);
    EXPECT_NEAR(view_fov::ScreenFov(90.0f, 4.0f / 3.0f, true), 90.0f, 0.001f);
}
