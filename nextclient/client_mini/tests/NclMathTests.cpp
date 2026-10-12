#include <gtest/gtest.h>

#include <array>
#include <vector>

#include <ncl_math/aabb.h>
#include <ncl_math/vec3.h>
#include <ncl_math/clip.h>

TEST(NclMathSegPointDistSq, ProjectionInsideSegment)
{
    const float a[3] = {0.0f, 0.0f, 0.0f};
    const float b[3] = {10.0f, 0.0f, 0.0f};
    const float p[3] = {5.0f, 3.0f, 0.0f};
    EXPECT_FLOAT_EQ(ncl_math::SegPointDistSq(a, b, p), 9.0f);
}

TEST(NclMathSegPointDistSq, ClampsToEndB)
{
    const float a[3] = {0.0f, 0.0f, 0.0f};
    const float b[3] = {10.0f, 0.0f, 0.0f};
    const float p[3] = {13.0f, 4.0f, 0.0f};
    EXPECT_FLOAT_EQ(ncl_math::SegPointDistSq(a, b, p), 25.0f);
}

TEST(NclMathSegPointDistSq, ClampsToEndA)
{
    const float a[3] = {0.0f, 0.0f, 0.0f};
    const float b[3] = {10.0f, 0.0f, 0.0f};
    const float p[3] = {-3.0f, 4.0f, 0.0f};
    EXPECT_FLOAT_EQ(ncl_math::SegPointDistSq(a, b, p), 25.0f);
}

TEST(NclMathSegPointDistSq, DegenerateSegment)
{
    const float a[3] = {2.0f, 2.0f, 2.0f};
    const float b[3] = {2.0f, 2.0f, 2.0f};
    const float p[3] = {2.0f, 2.0f, 5.0f};
    EXPECT_FLOAT_EQ(ncl_math::SegPointDistSq(a, b, p), 9.0f);
}

TEST(NclMathSegPointDistSq, PointOnSegment)
{
    const float a[3] = {0.0f, 0.0f, 0.0f};
    const float b[3] = {10.0f, 0.0f, 0.0f};
    const float p[3] = {5.0f, 0.0f, 0.0f};
    EXPECT_FLOAT_EQ(ncl_math::SegPointDistSq(a, b, p), 0.0f);
}

namespace
{
    void UnitSquare(ncl_math::UvVertex (&sq)[4])
    {
        sq[0] = {{0.0f, 0.0f}, {7.0f, 11.0f}};
        sq[1] = {{1.0f, 0.0f}, {9.0f, 11.0f}};
        sq[2] = {{1.0f, 1.0f}, {9.0f, 15.0f}};
        sq[3] = {{0.0f, 1.0f}, {7.0f, 15.0f}};
    }

    void ExpectHalfSquare(const ncl_math::UvVertex (&out)[5], const ncl_math::UvVertex (&expected)[4])
    {
        float doubled_area = 0.0f;

        for (int i = 0; i < 4; i++)
        {
            SCOPED_TRACE(i);
            EXPECT_FLOAT_EQ(out[i].pos.x, expected[i].pos.x);
            EXPECT_FLOAT_EQ(out[i].pos.y, expected[i].pos.y);
            EXPECT_FLOAT_EQ(out[i].uv.x, expected[i].uv.x);
            EXPECT_FLOAT_EQ(out[i].uv.y, expected[i].uv.y);

            const ncl_math::Vector2& a = out[i].pos;
            const ncl_math::Vector2& b = out[(i + 1) % 4].pos;
            doubled_area += a.x * b.y - b.x * a.y;
        }

        EXPECT_FLOAT_EQ(doubled_area * 0.5f, 0.5f);
    }
} // namespace

TEST(NclMathClipHalfPlane, KeepsGreaterHalfAndInterpolatesUv)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 0, 0.5f, true, out);

    ASSERT_EQ(n, 4);

    const ncl_math::UvVertex expected[] = {
        {{0.5f, 0.0f}, {8.0f, 11.0f}},
        {{1.0f, 0.0f}, {9.0f, 11.0f}},
        {{1.0f, 1.0f}, {9.0f, 15.0f}},
        {{0.5f, 1.0f}, {8.0f, 15.0f}},
    };

    ExpectHalfSquare(out, expected);
}

TEST(NclMathClipHalfPlane, KeepsLesserHalf)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 0, 0.5f, false, out);

    ASSERT_EQ(n, 4);

    const ncl_math::UvVertex expected[] = {
        {{0.0f, 0.0f}, {7.0f, 11.0f}},
        {{0.5f, 0.0f}, {8.0f, 11.0f}},
        {{0.5f, 1.0f}, {8.0f, 15.0f}},
        {{0.0f, 1.0f}, {7.0f, 15.0f}},
    };

    ExpectHalfSquare(out, expected);
}

TEST(NclMathClipHalfPlane, ClipsOnYAxis)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 1, 0.5f, true, out);

    ASSERT_EQ(n, 4);

    const ncl_math::UvVertex expected[] = {
        {{1.0f, 0.5f}, {9.0f, 13.0f}},
        {{1.0f, 1.0f}, {9.0f, 15.0f}},
        {{0.0f, 1.0f}, {7.0f, 15.0f}},
        {{0.0f, 0.5f}, {7.0f, 13.0f}},
    };

    ExpectHalfSquare(out, expected);
}

TEST(NclMathClipHalfPlane, FullyInsideKeepsEveryVertex)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 0, -1.0f, true, out);

    ASSERT_EQ(n, 4);
    for (int i = 0; i < n; i++)
    {
        EXPECT_EQ(out[i].pos, in[i].pos);
    }
}

TEST(NclMathClipHalfPlane, FullyOutsideKeepsNothing)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 0, 2.0f, true, out);

    EXPECT_EQ(n, 0);
}

TEST(NclMathClosestPoint, ProjectsClampsAndHandlesZeroLength)
{
    const ncl_math::Vector3 a{1.0f, 2.0f, 3.0f};
    const ncl_math::Vector3 b{11.0f, 2.0f, 3.0f};
    EXPECT_EQ(ncl_math::ClosestPointOnSegment(a, b, {6.0f, 7.0f, 3.0f}), (ncl_math::Vector3{6.0f, 2.0f, 3.0f}));
    EXPECT_EQ(ncl_math::ClosestPointOnSegment(a, b, {-2.0f, 2.0f, 3.0f}), a);
    EXPECT_EQ(ncl_math::ClosestPointOnSegment(a, b, {15.0f, 2.0f, 3.0f}), b);
    EXPECT_EQ(ncl_math::ClosestPointOnSegment(a, b, {6.0f, 2.0f, 3.0f}), (ncl_math::Vector3{6.0f, 2.0f, 3.0f}));
    EXPECT_EQ(ncl_math::ClosestPointOnSegment(a, a, {6.0f, 7.0f, 3.0f}), a);
    EXPECT_FLOAT_EQ(ncl_math::SegPointDistSq(a, a, {6.0f, 7.0f, 3.0f}), 50.0f);
    EXPECT_FLOAT_EQ(ncl_math::SegPointDistSq(a, b, {6.0f, 7.0f, 3.0f}), 25.0f);
}

TEST(NclMathAabb3, DistinguishesClosedInteriorAndHalfOpenBoundaries)
{
    const ncl_math::Aabb3 bounds{{1.0f, 2.0f, 3.0f}, {4.0f, 6.0f, 8.0f}};
    EXPECT_EQ(bounds.Center(), (ncl_math::Vector3{2.5f, 4.0f, 5.5f}));
    EXPECT_EQ(bounds.Size(), (ncl_math::Vector3{3.0f, 4.0f, 5.0f}));
    EXPECT_TRUE(bounds.Contains(bounds.max));
    EXPECT_TRUE(bounds.ContainsHalfOpen(bounds.min));
    EXPECT_FALSE(bounds.ContainsHalfOpen(bounds.max));
    EXPECT_FALSE(bounds.ContainsHalfOpen({2.0f, 6.0f, 4.0f}));
    const ncl_math::Aabb3 touching{{4.0f, 3.0f, 4.0f}, {5.0f, 4.0f, 5.0f}};
    EXPECT_TRUE(bounds.Overlaps(touching));
    EXPECT_FALSE(bounds.OverlapsInterior(touching));
    EXPECT_TRUE(bounds.OverlapsInterior({{3.0f, 3.0f, 4.0f}, {5.0f, 4.0f, 5.0f}}));
    EXPECT_FALSE(bounds.Overlaps({{5.0f, 3.0f, 4.0f}, {6.0f, 4.0f, 5.0f}}));
    EXPECT_EQ(bounds.ClosestPoint({0.0f, 4.0f, 10.0f}), (ncl_math::Vector3{1.0f, 4.0f, 8.0f}));
    EXPECT_EQ(bounds.ClosestPoint({2.0f, 4.0f, 5.0f}), (ncl_math::Vector3{2.0f, 4.0f, 5.0f}));
}

TEST(NclMathAabb3, AccumulatesFromFiniteEmptyBounds)
{
    ncl_math::Aabb3 bounds = ncl_math::Aabb3::Empty();
    EXPECT_FALSE(bounds.Contains({0.0f, 0.0f, 0.0f}));
    EXPECT_TRUE(std::isfinite(bounds.min.x));
    bounds.Include(ncl_math::Vector3{2.0f, 3.0f, 4.0f});
    bounds.Include(ncl_math::Aabb3{{-1.0f, 5.0f, 2.0f}, {3.0f, 6.0f, 8.0f}});
    EXPECT_EQ(bounds.min, (ncl_math::Vector3{-1.0f, 3.0f, 2.0f}));
    EXPECT_EQ(bounds.max, (ncl_math::Vector3{3.0f, 6.0f, 8.0f}));
}

TEST(NclMathHalfSpace, PreservesOrderAndBoundaryDuplicates)
{
    const std::array<double, 4> polygon{0.0, 1.0, 0.0, -1.0};
    std::vector<double> clipped;
    ncl_math::ClipPolygonHalfSpace<double>(
        polygon,
        [](double point) { return point; },
        [](double first, double second, double fraction) { return first + (second - first) * fraction; },
        [&](double point) { clipped.push_back(point); }
    );
    EXPECT_EQ(clipped, (std::vector<double>{0.0, 0.0, 1.0, 0.0, 0.0}));
}

TEST(NclMathHalfSpace, KeepsEmptyInsideAndOutsidePolygons)
{
    for (const std::vector<float>& polygon : {std::vector<float>{}, {1.0f, 2.0f, 3.0f}, {-1.0f, -2.0f, -3.0f}})
    {
        std::vector<float> clipped;
        ncl_math::ClipPolygonHalfSpace<float>(
            polygon,
            [](float point) { return point; },
            [](float first, float second, float fraction) { return first + (second - first) * fraction; },
            [&](float point) { clipped.push_back(point); }
        );
        EXPECT_EQ(clipped, polygon.empty() || polygon[0] < 0.0f ? std::vector<float>{} : polygon);
    }
}

TEST(NclMathHalfSpace, PreservesDoubleIntersectionsAndWinding)
{
    using Point_t = std::array<double, 2>;
    const std::array<Point_t, 4> polygon{{{-1.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {-1.0, 1.0}}};
    const double boundary = 1.0e-12;
    std::vector<Point_t> clipped;
    ncl_math::ClipPolygonHalfSpace<Point_t>(
        polygon,
        [&](const Point_t& point) { return point[0] - boundary; },
        [](const Point_t& first, const Point_t& second, double fraction) {
            return Point_t{first[0] + (second[0] - first[0]) * fraction, first[1] + (second[1] - first[1]) * fraction};
        },
        [&](const Point_t& point) { clipped.push_back(point); }
    );
    ASSERT_EQ(clipped.size(), 4u);
    EXPECT_NEAR(clipped[0][0], boundary, 2.0e-16);
    EXPECT_DOUBLE_EQ(clipped[0][1], 0.0);
    EXPECT_EQ(clipped[1], (Point_t{1.0, 0.0}));
    EXPECT_EQ(clipped[2], (Point_t{1.0, 1.0}));
    EXPECT_NEAR(clipped[3][0], boundary, 2.0e-16);
    EXPECT_DOUBLE_EQ(clipped[3][1], 1.0);
}

TEST(NclMathAabb2, ClosestPointPreservesInteriorAndClampsEdges)
{
    const ncl_math::Aabb2 bounds{{-2, -3}, {4, 5}};
    EXPECT_EQ(bounds.ClosestPoint({0, 1}), (ncl_math::Vector2{0, 1}));
    EXPECT_EQ(bounds.ClosestPoint({9, -8}), (ncl_math::Vector2{4, -3}));
    EXPECT_EQ(bounds.ClosestPoint({-9, 8}), (ncl_math::Vector2{-2, 5}));

    for (ncl_math::Vector2 corner : {ncl_math::Vector2{-2, -3}, {-2, 5}, {4, -3}, {4, 5}})
    {
        EXPECT_EQ(bounds.ClosestPoint(corner), corner);
    }

    const ncl_math::Aabb2 point_bounds{{2, 3}, {2, 3}};

    for (ncl_math::Vector2 point : {ncl_math::Vector2{-9, -8}, {0, 1}, {2, 3}, {9, 8}})
    {
        EXPECT_EQ(point_bounds.ClosestPoint(point), (ncl_math::Vector2{2, 3}));
    }
}
