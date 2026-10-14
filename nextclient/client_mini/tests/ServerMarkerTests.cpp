#include <gtest/gtest.h>

#include "hud/radar/markers/ServerMarker.h"

TEST(ServerMarker, EasesHeightWithXy)
{
    ncl_math::Vector3 pos = ServerMarker_Ease({0, 0, 0}, {64, 64, 64}, 0.05f);

    EXPECT_GT(pos.z, 0.0f);
    EXPECT_LT(pos.z, 64.0f);
    EXPECT_FLOAT_EQ(pos.z, pos.x);
    EXPECT_FLOAT_EQ(pos.z, pos.y);
}

TEST(ServerMarker, VerticalTeleportSnaps)
{
    EXPECT_EQ(ServerMarker_Ease({64, 64, 1}, {64, 64, 257}, 0.05f), (ncl_math::Vector3{64, 64, 257}));
}
