#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>

#include <gtest/gtest.h>
#include <ncl_math/vec3.h>

TEST(VectorNormalization, PreservesScalarDivisionForFiniteAndSpecialInputs)
{
    const std::array<float, 12> values = {
        0.0f,
        -0.0f,
        std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),
        1e-20f,
        -1e-20f,
        1.0f,
        -1.0f,
        1e20f,
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()
    };

    for (float x : values)
    {
        for (float y : values)
        {
            for (float z : values)
            {
                const ncl_math::Vector3 input{x, y, z};
                const float length = std::sqrt(x * x + y * y + z * z);
                const ncl_math::Vector3 expected =
                    length > 0.0f ? ncl_math::Vector3{x / length, y / length, z / length} : ncl_math::Vector3{};
                const ncl_math::Vector3 actual = input.Normalized();

                for (int axis = 0; axis < 3; ++axis)
                {
                    if (std::isnan(expected[axis]))
                    {
                        EXPECT_TRUE(std::isnan(actual[axis]));
                    }
                    else
                    {
                        EXPECT_EQ(std::bit_cast<uint32_t>(actual[axis]), std::bit_cast<uint32_t>(expected[axis]));
                    }
                }
            }
        }
    }
}

TEST(VectorNormalization, PreservesScalarDivisionForSeededInputs)
{
    std::mt19937 random(7193);
    std::uniform_real_distribution<float> distribution(-4096.0f, 4096.0f);

    for (int i = 0; i < 100000; ++i)
    {
        const ncl_math::Vector3 input{distribution(random), distribution(random), distribution(random)};
        const float length = std::sqrt(input.x * input.x + input.y * input.y + input.z * input.z);
        const ncl_math::Vector3 expected{input.x / length, input.y / length, input.z / length};
        const ncl_math::Vector3 actual = input.Normalized();

        for (int axis = 0; axis < 3; ++axis)
        {
            ASSERT_EQ(std::bit_cast<uint32_t>(actual[axis]), std::bit_cast<uint32_t>(expected[axis]));
        }
    }
}

TEST(Trigonometry, VectorDirectionsPreserveAngleConventions)
{
    std::mt19937 random(821);
    std::uniform_real_distribution<float> distribution(-7200.0f, 7200.0f);

    for (int i = 0; i < 10000; ++i)
    {
        const float angle = distribution(random);
        const ncl_math::Vector2 direction = ncl_math::FromAngle(angle);
        const ncl_math::Vector2 rotated = ncl_math::Vector2{0.25f, -0.75f}.Rotated(angle);

        ASSERT_NEAR(direction.x, std::cos(angle), 6e-8f);
        ASSERT_NEAR(direction.y, std::sin(angle), 6e-8f);
        ASSERT_NEAR(rotated.x, 0.25f * std::cos(angle) + 0.75f * std::sin(angle), 2e-7f);
        ASSERT_NEAR(rotated.y, 0.25f * std::sin(angle) - 0.75f * std::cos(angle), 2e-7f);

        const float angles[] = {distribution(random), distribution(random), distribution(random)};
        const float sp = std::sin(angles[0] * ncl_math::kDeg2Rad);
        const float cp = std::cos(angles[0] * ncl_math::kDeg2Rad);
        const float sy = std::sin(angles[1] * ncl_math::kDeg2Rad);
        const float cy = std::cos(angles[1] * ncl_math::kDeg2Rad);
        const float sr = std::sin(angles[2] * ncl_math::kDeg2Rad);
        const float cr = std::cos(angles[2] * ncl_math::kDeg2Rad);
        const std::array expected = {
            cp * cy,
            cp * sy,
            -sp,
            -sr * sp * cy + cr * sy,
            -sr * sp * sy - cr * cy,
            -sr * cp,
            cr * sp * cy + sr * sy,
            cr * sp * sy - sr * cy,
            cr * cp,
        };
        std::array<float, 9> actual{};
        ncl_math::AngleVectors(angles, actual.data(), actual.data() + 3, actual.data() + 6);

        for (size_t component = 0; component < actual.size(); ++component)
        {
            ASSERT_NEAR(actual[component], expected[component], 3e-7f);
        }

        float forward[3]{};
        ncl_math::AngleVectors(angles, forward, nullptr, nullptr);
        EXPECT_EQ(std::memcmp(forward, actual.data(), sizeof(forward)), 0);
    }
}
