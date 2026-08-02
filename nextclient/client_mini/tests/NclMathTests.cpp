#include <gtest/gtest.h>

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
    // Unit square, CCW, uv laid over pos so a correct clip keeps uv == pos at every
    // vertex, including the interpolated crossings.
    void UnitSquare(ncl_math::UvVertex (&sq)[4])
    {
        sq[0] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
        sq[1] = {{1.0f, 0.0f}, {1.0f, 0.0f}};
        sq[2] = {{1.0f, 1.0f}, {1.0f, 1.0f}};
        sq[3] = {{0.0f, 1.0f}, {0.0f, 1.0f}};
    }
} // namespace

TEST(NclMathClipHalfPlane, KeepsGreaterHalfAndInterpolatesUv)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 4, 0, 0.5f, true, out);

    EXPECT_EQ(n, 4);
    for (int i = 0; i < n; i++)
    {
        EXPECT_GE(out[i].pos.x, 0.5f - 1e-4f); // nothing kept left of the cut
        EXPECT_FLOAT_EQ(out[i].uv.x, out[i].pos.x); // uv interpolated in lockstep with pos
        EXPECT_FLOAT_EQ(out[i].uv.y, out[i].pos.y);
    }
}

TEST(NclMathClipHalfPlane, KeepsLesserHalf)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 4, 0, 0.5f, false, out);

    EXPECT_EQ(n, 4);
    for (int i = 0; i < n; i++)
    {
        EXPECT_LE(out[i].pos.x, 0.5f + 1e-4f);
    }
}

TEST(NclMathClipHalfPlane, ClipsOnYAxis)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 4, 1, 0.5f, true, out);

    EXPECT_EQ(n, 4);
    for (int i = 0; i < n; i++)
    {
        EXPECT_GE(out[i].pos.y, 0.5f - 1e-4f);
    }
}

TEST(NclMathClipHalfPlane, FullyInsideKeepsEveryVertex)
{
    ncl_math::UvVertex in[4];
    UnitSquare(in);

    ncl_math::UvVertex out[5];
    int n = ncl_math::ClipHalfPlane(in, 4, 0, -1.0f, true, out);

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
    int n = ncl_math::ClipHalfPlane(in, 4, 0, 2.0f, true, out);

    EXPECT_EQ(n, 0);
}
