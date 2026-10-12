#include <gtest/gtest.h>

#include "hud/radar/layers/players/DeathMarks.h"

TEST(DeathMarks, KeepsRecordedHeight)
{
    DeathMarks deaths;
    deaths.Add({64, 64, 256}, {255, 0, 0}, 10.0f);
    DeathMarks::Visible visible[1];

    ASSERT_EQ(deaths.CollectVisible(visible, 11.0f), 1);
    EXPECT_FLOAT_EQ(visible[0].pos.z, 256.0f);
}
