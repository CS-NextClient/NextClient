#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>
#include <ncl_math/Matrix3x4.h>

namespace
{
    constexpr float kQuaternionTolerance = 2e-6f;
    constexpr float kHalfPi = std::numbers::pi_v<float> / 2.0f;

    void ExpectQuaternion(const ncl_math::Quaternion& actual, const std::array<float, 4>& expected)
    {
        for (int coordinate = 0; coordinate < 4; ++coordinate)
        {
            EXPECT_NEAR(actual[coordinate], expected[coordinate], kQuaternionTolerance) << coordinate;
        }
    }
} // namespace

TEST(QuaternionMatrixMath, AngleIdentityAndAxisOrder)
{
    const ncl_math::Vector3 identity{};
    ncl_math::Quaternion quaternion = {7, 7, 7, 7};
    ncl_math::AngleQuaternion(identity, quaternion);
    ExpectQuaternion(quaternion, {0, 0, 0, 1});

    const ncl_math::Vector3 z_rotation = {0, 0, kHalfPi};
    ncl_math::AngleQuaternion(z_rotation, quaternion);
    ExpectQuaternion(quaternion, {0, 0, 0.7071067812f, 0.7071067812f});

    const ncl_math::Vector3 combined_rotation = {kHalfPi, kHalfPi, 0};
    ncl_math::AngleQuaternion(combined_rotation, quaternion);
    ExpectQuaternion(quaternion, {0.5f, 0.5f, -0.5f, 0.5f});
}

TEST(QuaternionMatrixMath, QuaternionMatrixPreservesTranslationAndUnnormalizedInput)
{
    const ncl_math::Quaternion half_turn = {0, 0, 1, 0};
    ncl_math::Matrix3x4 output = {{
        {7, 7, 7, 7},
        {8, 8, 8, 8},
        {9, 9, 9, 9}
    }};
    ncl_math::QuaternionMatrix(half_turn, output);
    const ncl_math::Matrix3x4 expected = {{
        {-1, 0, 0, 7},
        {0, -1, 0, 8},
        {0, 0, 1, 9}
    }};

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            EXPECT_EQ(output[row][column], expected[row][column]);
        }
    }

    const ncl_math::Quaternion unnormalized = {0, 0, 0, 2};
    ncl_math::QuaternionMatrix(unnormalized, output);

    EXPECT_EQ(output[0][0], 1.0f);
    EXPECT_EQ(output[1][1], 1.0f);
    EXPECT_EQ(output[2][2], 1.0f);
}

TEST(QuaternionMatrixMath, TransformCompositionPreservesParentOrder)
{
    const ncl_math::Matrix3x4 parent = {{
        {1, 0, 0, 10},
        {0, 1, 0, 20},
        {0, 0, 1, 30}
    }};
    const ncl_math::Matrix3x4 child = {{
        {0, -1, 0, 1},
        {1, 0, 0, 2},
        {0, 0, 1, 3}
    }};
    ncl_math::Matrix3x4 output{};
    ncl_math::ConcatTransforms(parent, child, output);
    const ncl_math::Matrix3x4 expected = {{
        {0, -1, 0, 11},
        {1, 0, 0, 22},
        {0, 0, 1, 33}
    }};

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            EXPECT_EQ(output[row][column], expected[row][column]);
        }
    }

    ncl_math::ConcatTransforms(child, parent, output);

    EXPECT_EQ(output[0][3], -19.0f);
    EXPECT_EQ(output[1][3], 12.0f);
    EXPECT_EQ(output[2][3], 33.0f);
}

TEST(QuaternionMatrixMath, QuaternionMatrixPreservesCrossTermSigns)
{
    const ncl_math::Quaternion quaternion = {1, 2, 3, 4};
    ncl_math::Matrix3x4 output{};
    ncl_math::QuaternionMatrix(quaternion, output);
    const float expected[3][3] = {{-25, -20, 22}, {28, -19, 4}, {-10, 20, -9}};

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            EXPECT_EQ(output[row][column], expected[row][column]);
        }
    }
}

TEST(QuaternionMatrixMath, TransformCompositionReadsAliasedInputsAfterEachStore)
{
    ncl_math::Matrix3x4 parent = {{
        {1, 0, 0, 10},
        {0, 1, 0, 20},
        {0, 0, 1, 30}
    }};
    const ncl_math::Matrix3x4 child = {{
        {0, -1, 0, 1},
        {1, 0, 0, 2},
        {0, 0, 1, 3}
    }};
    ncl_math::ConcatTransforms(parent, child, parent);
    const ncl_math::Matrix3x4 expected = {{
        {0, 0, 0, 10},
        {1, -1, 0, 19},
        {0, 0, 1, 33}
    }};

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            EXPECT_EQ(parent[row][column], expected[row][column]);
        }
    }
}

TEST(QuaternionMatrixMath, ValueCopyIncludesTranslation)
{
    const ncl_math::Matrix3x4 source = {{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12}
    }};
    ncl_math::Matrix3x4 destination{};
    destination = source;

    EXPECT_EQ(std::memcmp(&source, &destination, sizeof(source)), 0);
}

TEST(MatrixMath, CompositionPreservesInPlaceStoreOrder)
{
    for (int alias = 0; alias < 3; ++alias)
    {
        ncl_math::Matrix3x4 expected_left = {{
            {1, 2, 3, 4},
            {5, 6, 7, 8},
            {9, 10, 11, 12}
        }};
        ncl_math::Matrix3x4 expected_right = {{
            {2, 0, 1, 3},
            {1, 3, 0, 4},
            {0, 1, 4, 5}
        }};
        ncl_math::Matrix3x4 actual_left = expected_left;
        ncl_math::Matrix3x4 actual_right = expected_right;
        const ncl_math::Matrix3x4& right = alias == 2 ? expected_left : expected_right;
        ncl_math::Matrix3x4& output = alias == 1 ? expected_right : expected_left;

        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                const float first = expected_left[row][0] * right[0][column];
                const float second = expected_left[row][1] * right[1][column];
                const float third = expected_left[row][2] * right[2][column];
                float value = (first + second) + third;

                if (column == 3)
                {
                    value += expected_left[row][3];
                }

                output[row][column] = value;
            }
        }

        ncl_math::ConcatTransforms(actual_left, alias == 2 ? actual_left : actual_right, alias == 1 ? actual_right : actual_left);

        EXPECT_EQ(std::memcmp(&expected_left, &actual_left, sizeof(expected_left)), 0) << alias;
        EXPECT_EQ(std::memcmp(&expected_right, &actual_right, sizeof(expected_right)), 0) << alias;
    }
}
TEST(QuaternionMatrixMath, TransformCompositionPreservesSignedZero)
{
    ncl_math::Matrix3x4 left{};
    ncl_math::Matrix3x4 right{};
    ncl_math::Matrix3x4 output{};

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            left[row][column] = -0.0f;
            right[row][column] = 1.0f;
        }
    }

    ncl_math::ConcatTransforms(left, right, output);

    for (const auto& row : output.rows)
    {
        for (float value : row)
        {
            EXPECT_EQ(value, 0.0f);
            EXPECT_TRUE(std::signbit(value));
        }
    }
}

TEST(QuaternionMath, SlerpMutatesShortestPathInputEvenAtZeroWeight)
{
    const ncl_math::Quaternion first = {0, 0, 0, 1};

    for (float blend : {0.0f, 0.25f})
    {
        ncl_math::Quaternion second = {0, 0, 0, -1};
        ncl_math::Quaternion output = {7, 7, 7, 7};
        ncl_math::QuaternionSlerp(first, second, blend, output);
        ExpectQuaternion(output, {0, 0, 0, 1});

        EXPECT_EQ(second[3], 1.0f);
        EXPECT_TRUE(std::signbit(second[0]));
        EXPECT_TRUE(std::signbit(second[1]));
        EXPECT_TRUE(std::signbit(second[2]));
    }
}

TEST(QuaternionMath, SlerpPreservesDistanceTieAndDoesNotClampBlend)
{
    const ncl_math::Quaternion first = {0, 0, 0, 1};
    ncl_math::Quaternion second = {0, 0, 1, 0};
    ncl_math::Quaternion output{};
    ncl_math::QuaternionSlerp(first, second, 0.5f, output);
    ExpectQuaternion(output, {0, 0, 0.7071067812f, 0.7071067812f});

    EXPECT_EQ(second[2], 1.0f);
    EXPECT_FALSE(std::signbit(second[0]));

    ncl_math::QuaternionSlerp(first, second, 1.5f, output);
    ExpectQuaternion(output, {0, 0, 0.7071067812f, -0.7071067812f});
}

TEST(QuaternionMath, SlerpDoesNotNormalizeNearEqualInputs)
{
    const ncl_math::Quaternion first = {2, 0, 0, 0};
    ncl_math::Quaternion second = {2, 0, 0, 0};
    ncl_math::Quaternion output{};
    ncl_math::QuaternionSlerp(first, second, 0.25f, output);
    ExpectQuaternion(output, {2, 0, 0, 0});
}

TEST(QuaternionMath, SlerpShortestPathPreservesSmallDistanceDifference)
{
    const float small = 0x1p-10f;
    const ncl_math::Quaternion first = {1, small, 0, 0};
    ncl_math::Quaternion second = {std::nextafter(small, 0.0f), -1, 0, 0};
    ncl_math::Quaternion output{};
    ncl_math::QuaternionSlerp(first, second, 0.5f, output);

    EXPECT_LT(second[0], 0);
    EXPECT_EQ(second[1], 1);
    EXPECT_GT(output[0], 0);
    EXPECT_GT(output[1], 0);
}

TEST(QuaternionMath, LongChainStaysWithinPoseToleranceAroundSlerpThreshold)
{
    const ncl_math::Quaternion first = {0, 0, 0, 1};

    for (float angle : {0.000001f, 0.0028f, 0.0029f, 0.03f})
    {
        for (float blend : {0.0f, 0.1f, 0.37f, 0.5f, 1.0f})
        {
            const ncl_math::Vector3 angles = {0, 0, angle};
            ncl_math::Quaternion second{};
            ncl_math::Quaternion quaternion{};
            ncl_math::AngleQuaternion(angles, second);
            ncl_math::QuaternionSlerp(first, second, blend, quaternion);

            ncl_math::Matrix3x4 local = {{
                {0, 0, 0, 16},
                {0, 0, 0, 0},
                {0, 0, 0, 0}
            }};
            ncl_math::Matrix3x4 parent = {{
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0}
            }};
            ncl_math::QuaternionMatrix(quaternion, local);
            double expected_x = 0;
            double expected_y = 0;
            const double step = double(angle) * blend;

            for (int bone = 0; bone < 128; ++bone)
            {
                ncl_math::Matrix3x4 output{};
                ncl_math::ConcatTransforms(parent, local, output);
                parent = output;
                expected_x += 16 * std::cos(bone * step);
                expected_y += 16 * std::sin(bone * step);
            }

            const double cosine = std::cos(128 * step);
            const double sine = std::sin(128 * step);

            EXPECT_NEAR(parent[0][0], cosine, 1e-4);
            EXPECT_NEAR(parent[0][1], -sine, 1e-4);
            EXPECT_NEAR(parent[1][0], sine, 1e-4);
            EXPECT_NEAR(parent[1][1], cosine, 1e-4);

            const double actual_x = double(parent[0][0]) * 64 - double(parent[0][1]) * 64 + parent[0][3];
            const double actual_y = double(parent[1][0]) * 64 - double(parent[1][1]) * 64 + parent[1][3];
            const double delta_x = actual_x - (cosine * 64 + sine * 64 + expected_x);
            const double delta_y = actual_y - (sine * 64 - cosine * 64 + expected_y);

            EXPECT_LE(std::hypot(delta_x, delta_y), 0.01) << angle << ", " << blend;
        }
    }
}

TEST(QuaternionMath, SlerpFallbackKeepsFourthCoordinateFromSecondZ)
{
    const ncl_math::Quaternion first = {std::numeric_limits<float>::quiet_NaN(), 0, 0, 1};
    ncl_math::Quaternion second = {1, 2, 3, 4};
    ncl_math::Quaternion output{};
    ncl_math::QuaternionSlerp(first, second, 0.5f, output);

    EXPECT_TRUE(std::isnan(output[0]));
    EXPECT_NEAR(output[1], 0.7071067812f, kQuaternionTolerance);
    EXPECT_NEAR(output[2], -2.8284271247f, kQuaternionTolerance);
    EXPECT_EQ(output[3], 3.0f);
}
