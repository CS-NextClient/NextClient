#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

#include "hud/radar/render/RadarPolyline.h"

namespace
{
    struct Quad
    {
        size_t index;
        std::array<ncl_math::Vector2, 4> corners;
    };

    std::vector<Quad> Quads(std::span<const ncl_math::Vector2> points)
    {
        std::vector<Quad> quads;
        RadarPolyline_ForEachQuad(points, 2.0f, [&](size_t index, const std::array<ncl_math::Vector2, 4>& corners) {
            quads.push_back({index, corners});
        });
        return quads;
    }
} // namespace

TEST(RadarPolyline, StraightSegmentsShareTheJoin)
{
    const ncl_math::Vector2 points[] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {20.0f, 0.0f}};
    std::vector<Quad> quads = Quads(points);
    ASSERT_EQ(quads.size(), 2u);
    EXPECT_EQ(quads[0].corners, (std::array<ncl_math::Vector2, 4>{{{0.0f, 1.0f}, {0.0f, -1.0f}, {10.0f, -1.0f}, {10.0f, 1.0f}}}));
    EXPECT_EQ(quads[1].corners[0], quads[0].corners[3]);
    EXPECT_EQ(quads[1].corners[1], quads[0].corners[2]);
}

TEST(RadarPolyline, RightAngleUsesTheSameMiterOnBothSegments)
{
    const ncl_math::Vector2 points[] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}};
    std::vector<Quad> quads = Quads(points);
    ASSERT_EQ(quads.size(), 2u);
    EXPECT_NEAR(quads[0].corners[2].x, 11.0f, 0.00001f);
    EXPECT_NEAR(quads[0].corners[2].y, -1.0f, 0.00001f);
    EXPECT_NEAR(quads[0].corners[3].x, 9.0f, 0.00001f);
    EXPECT_NEAR(quads[0].corners[3].y, 1.0f, 0.00001f);
    EXPECT_EQ(quads[1].corners[0], quads[0].corners[3]);
    EXPECT_EQ(quads[1].corners[1], quads[0].corners[2]);
}

TEST(RadarPolyline, ReversalUsesNextPerpendicularAndClampsSharpTurns)
{
    const ncl_math::Vector2 reverse[] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {0.0f, 0.0f}};
    std::vector<Quad> quads = Quads(reverse);
    ASSERT_EQ(quads.size(), 2u);
    EXPECT_EQ(quads[0].corners[3], (ncl_math::Vector2{10.0f, -1.0f}));
    EXPECT_EQ(quads[1].corners[0], quads[0].corners[3]);
    const ncl_math::Vector2 sharp[] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {0.0f, 1.0f}};
    quads = Quads(sharp);
    ASSERT_EQ(quads.size(), 2u);
    EXPECT_NEAR((quads[0].corners[3] - sharp[1]).Length(), 2.0f, 0.00001f);
}

TEST(RadarPolyline, RepeatedAndShortSegmentsBreakContinuityAndKeepSourceIndices)
{
    const ncl_math::Vector2 points[] = {{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {10.0f, 10.0001f}, {20.0f, 10.0001f}};
    std::vector<Quad> quads = Quads(points);
    ASSERT_EQ(quads.size(), 3u);
    EXPECT_EQ(quads[0].index, 0u);
    EXPECT_EQ(quads[1].index, 2u);
    EXPECT_EQ(quads[2].index, 4u);
    EXPECT_EQ(quads[1].corners[0], (ncl_math::Vector2{9.0f, 0.0f}));
    EXPECT_EQ(quads[1].corners[3], (ncl_math::Vector2{9.0f, 10.0f}));
}

TEST(RadarPolyline, ExactMinimumLengthSurvivesAndEmptyInputsEmitNothing)
{
    const ncl_math::Vector2 short_points[] = {{0.0f, 0.0f}, {std::nextafter(0.001f, 0.0f), 0.0f}};
    EXPECT_TRUE(Quads(short_points).empty());
    const ncl_math::Vector2 exact[] = {{0.0f, 0.0f}, {0.001f, 0.0f}};
    ASSERT_EQ(Quads(exact).size(), 1u);
    EXPECT_TRUE(Quads({}).empty());
    EXPECT_TRUE(Quads({exact, 1}).empty());
}
