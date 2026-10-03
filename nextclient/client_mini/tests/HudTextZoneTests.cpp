#include <gtest/gtest.h>

#include "HudTextZone.h"

class HudTextZoneTest : public testing::Test
{
protected:
    void SetUp() override
    {
        HudTextZone_BeginFrame(1024, 768);
        HudTextZone_SetPassthrough(false);
        HudTextZone_SetZone(20.0f, 20.0f, 180.0f);
    }

    void TearDown() override
    {
        HudTextZone_ClearZone();
    }
};

TEST_F(HudTextZoneTest, AdjacentRowsKeepTheirSharedLeftAnchor)
{
    HudTextLine lines[] = {{14, 185, 170}, {14, 207, 150}};
    HudTextZone_ComputeBlockShifts(lines);

    EXPECT_EQ(lines[0].left + lines[0].dx, 212);
    EXPECT_EQ(lines[1].left + lines[1].dx, 212);
    EXPECT_EQ(lines[0].dy, 0);
    EXPECT_EQ(lines[1].dy, 0);
}

TEST_F(HudTextZoneTest, MatchingAnchorsDoNotConnectDistantBlocks)
{
    HudTextLine lines[] = {{14, 185, 170}, {14, 300, 150}};
    HudTextZone_ComputeBlockShifts(lines);

    EXPECT_GT(lines[0].dx, 0);
    EXPECT_EQ(lines[1].dx, 0);
    EXPECT_EQ(lines[1].dy, 0);
}

TEST_F(HudTextZoneTest, NearbyRowsWithDifferentAnchorsRemainIndependent)
{
    HudTextLine lines[] = {{14, 185, 170}, {30, 225, 150}};
    HudTextZone_ComputeBlockShifts(lines);

    EXPECT_GT(lines[0].dx, 0);
    EXPECT_EQ(lines[1].dx, 0);
}

TEST_F(HudTextZoneTest, WidestRowMovesTheWholeBlockBelowWhenItCannotFitBeside)
{
    HudTextLine lines[] = {{14, 185, 170}, {14, 207, 900}, {14, 229, 150}};
    HudTextZone_ComputeBlockShifts(lines);

    for (const HudTextLine& line : lines)
    {
        EXPECT_EQ(line.dx, 0);
        EXPECT_EQ(line.dy, 27);
    }
}

TEST_F(HudTextZoneTest, TextInTheMarginIsMovedClearOfTheRim)
{
    HudTextLine lines[] = {{205, 100, 100}};
    HudTextZone_ComputeBlockShifts(lines);

    EXPECT_EQ(lines[0].left + lines[0].dx, 212);
}

TEST_F(HudTextZoneTest, PassthroughAndHiddenRadarLeaveTextAtItsOriginalPosition)
{
    HudTextLine lines[] = {{14, 185, 170}, {14, 207, 150}};
    HudTextZone_SetPassthrough(true);
    HudTextZone_ComputeBlockShifts(lines);

    EXPECT_EQ(lines[0].dx, 0);
    EXPECT_EQ(lines[1].dx, 0);

    HudTextZone_SetPassthrough(false);
    HudTextZone_ClearZone();
    HudTextZone_ComputeBlockShifts(lines);

    EXPECT_EQ(lines[0].dx, 0);
    EXPECT_EQ(lines[1].dx, 0);
}

TEST_F(HudTextZoneTest, BlockStartingAboveTheRadarClearsItsBottomEdge)
{
    HudTextLine lines[] = {{14, 0, 900}, {14, 24, 900}};
    HudTextZone_ComputeBlockShifts(lines);

    EXPECT_EQ(lines[0].y + lines[0].dy, 212);
    EXPECT_EQ(lines[1].y + lines[1].dy, 236);
    EXPECT_EQ(lines[0].dx, 0);
    EXPECT_EQ(lines[1].dx, 0);
}
