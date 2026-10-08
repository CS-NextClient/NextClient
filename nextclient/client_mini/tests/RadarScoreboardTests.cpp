#include <vector>

#include <gtest/gtest.h>

#include "RadarScoreboard.h"

namespace
{
    const ncl_math::Aabb2 kRadar{{20.0f, 20.0f}, {264.0f, 264.0f}};
    const ncl_math::Aabb2 kTable{{120.0f, 140.0f}, {1160.0f, 820.0f}};
    const ncl_math::Vector2 kScreen{1280.0f, 960.0f};
} // namespace

TEST(RadarScoreboard, FourByThreeMovesBelowRadarWithoutResizingOrClipping)
{
    RadarScoreboard placement;
    EXPECT_EQ(placement.Update(1, kTable, kScreen, &kRadar), (ncl_math::Vector2{120.0f, 274.0f}));
}

TEST(RadarScoreboard, MovesRightWhenThereIsEnoughHorizontalSpace)
{
    RadarScoreboard placement;
    ncl_math::Aabb2 table{{120.0f, 140.0f}, {1060.0f, 820.0f}};
    EXPECT_EQ(placement.Update(1, table, kScreen, &kRadar), (ncl_math::Vector2{274.0f, 140.0f}));
}

TEST(RadarScoreboard, NonOverlappingTableKeepsItsPosition)
{
    RadarScoreboard placement;
    ncl_math::Aabb2 table{{500.0f, 140.0f}, {1540.0f, 820.0f}};
    EXPECT_EQ(placement.Update(1, table, {1920.0f, 1080.0f}, &kRadar), table.min);
}

TEST(RadarScoreboard, InsufficientSpaceKeepsTheTableOnScreen)
{
    RadarScoreboard placement;
    ncl_math::Aabb2 radar{{20.0f, 20.0f}, {600.0f, 600.0f}};
    EXPECT_EQ(placement.Update(1, kTable, kScreen, &radar), kTable.min);
}

TEST(RadarScoreboard, RepeatedFramesDoNotAccumulateTheOffsetAndDisablingRestoresIt)
{
    RadarScoreboard placement;
    ncl_math::Vector2 pos = placement.Update(1, kTable, kScreen, &kRadar);
    for (int frame = 0; frame < 3; frame++)
    {
        pos = placement.Update(1, {pos, pos + kTable.Size()}, kScreen, &kRadar);
        EXPECT_EQ(pos, (ncl_math::Vector2{120.0f, 274.0f}));
    }
    EXPECT_EQ(placement.Update(1, {pos, pos + kTable.Size()}, kScreen, nullptr), kTable.min);
    EXPECT_EQ(placement.Update(1, kTable, kScreen, nullptr), kTable.min);
}

TEST(RadarScoreboard, SmallerRadarRestoresTheOriginalPosition)
{
    RadarScoreboard placement;
    ncl_math::Vector2 pos = placement.Update(1, kTable, kScreen, &kRadar);
    ncl_math::Aabb2 small_radar{{20.0f, 20.0f}, {110.0f, 110.0f}};
    EXPECT_EQ(placement.Update(1, {pos, pos + kTable.Size()}, kScreen, &small_radar), kTable.min);
}

TEST(RadarScoreboard, StockRecenteringDoesNotLoseTheRestorePosition)
{
    RadarScoreboard placement;
    placement.Update(1, kTable, kScreen, &kRadar);
    ncl_math::Vector2 pos = placement.Update(1, kTable, kScreen, &kRadar);
    EXPECT_EQ(pos, (ncl_math::Vector2{120.0f, 274.0f}));
    EXPECT_EQ(placement.Update(1, {pos, pos + kTable.Size()}, kScreen, nullptr), kTable.min);
}

TEST(RadarScoreboard, ANewPanelDoesNotInheritTheOldPanelsPosition)
{
    RadarScoreboard placement;
    ncl_math::Vector2 pos = placement.Update(1, kTable, kScreen, &kRadar);
    EXPECT_EQ(placement.Update(2, {pos, pos + kTable.Size()}, kScreen, nullptr), pos);
}

TEST(RadarScoreboard, ExternalMovesAndResolutionChangesTakePrecedenceOverRestoration)
{
    RadarScoreboard placement;
    ncl_math::Vector2 pos = placement.Update(1, kTable, kScreen, &kRadar);
    ncl_math::Aabb2 moved{{100.0f, 200.0f}, {1140.0f, 880.0f}};
    EXPECT_EQ(placement.Update(1, moved, kScreen, nullptr), moved.min);
    pos = placement.Update(1, kTable, kScreen, &kRadar);
    EXPECT_EQ(placement.Update(1, {pos, pos + kTable.Size()}, {1920.0f, 1080.0f}, nullptr), pos);
}

namespace
{
    struct PanelNode
    {
        const char* name;
        bool visible;
        std::vector<std::uint32_t> children;
    };

    struct PanelTree
    {
        std::vector<PanelNode> nodes;
        bool IsVisible(std::uint32_t panel) const
        {
            return nodes.at(panel).visible;
        }
        const char* GetName(std::uint32_t panel) const
        {
            return nodes.at(panel).name;
        }
        int GetChildCount(std::uint32_t panel) const
        {
            return (int)nodes.at(panel).children.size();
        }
        std::uint32_t GetChild(std::uint32_t panel, int index) const
        {
            return nodes.at(panel).children.at(index);
        }
    };
} // namespace

TEST(RadarScoreboard, HiddenTemplateDoesNotMaskTheVisibleScoreboard)
{
    PanelTree panels{{
        {"", false, {}},
        {"root", true, {2, 3}},
        {"ClientScoreBoard", false, {}},
        {"ClientScoreBoard", true, {}},
    }};
    EXPECT_EQ(RadarScoreboard_FindVisiblePanel(panels, 1), 3u);
    panels.nodes[3].visible = false;
    EXPECT_EQ(RadarScoreboard_FindVisiblePanel(panels, 1), 0u);
}

TEST(RadarScoreboard, ScoreboardUnderAHiddenParentIsNotSelected)
{
    PanelTree panels{{
        {"", false, {}},
        {"root", true, {2, 4}},
        {"inactive viewport", false, {3}},
        {"ClientScoreBoard", true, {}},
        {"ClientScoreBoard", true, {}},
    }};
    EXPECT_EQ(RadarScoreboard_FindVisiblePanel(panels, 1), 4u);
}
