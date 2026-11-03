#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>

#if defined(_MSC_VER) && defined(_M_IX86)
#include <float.h>
#endif

#include <gtest/gtest.h>

#include <ncl_math/trigonometry.h>

TEST(Trigonometry, SinCosPreservesSignedZero)
{
    for (float angle : {0.0f, -0.0f})
    {
        float sine, cosine;
        ncl_math::SinCos(angle, sine, cosine);

        EXPECT_EQ(std::bit_cast<uint32_t>(sine), std::bit_cast<uint32_t>(angle));
        EXPECT_EQ(cosine, 1.0f);
    }
}

TEST(Trigonometry, SinCosMatchesLibmAcrossReductionBoundaryAndFloatRange)
{
    const std::array angles = {
        std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),
        std::nextafter(8192.0f, 0.0f),
        8192.0f,
        std::nextafter(8192.0f, INFINITY),
        1e20f,
        std::numeric_limits<float>::max(),
    };

    for (float magnitude : angles)
    {
        for (float angle : {magnitude, -magnitude})
        {
            float sine, cosine;
            ncl_math::SinCos(angle, sine, cosine);

            EXPECT_NEAR(sine, std::sin(angle), 6e-8f) << angle;
            EXPECT_NEAR(cosine, std::cos(angle), 6e-8f) << angle;
        }
    }
}

TEST(Trigonometry, SinCosMatchesLibmForSeededFiniteInputs)
{
    std::mt19937 random(92731);
    std::uniform_real_distribution<float> distribution(-8192.0f, 8192.0f);

    for (int i = 0; i < 100000; ++i)
    {
        const float angle = distribution(random);
        float sine, cosine;
        ncl_math::SinCos(angle, sine, cosine);

        ASSERT_NEAR(sine, std::sin(angle), 6e-8f) << angle;
        ASSERT_NEAR(cosine, std::cos(angle), 6e-8f) << angle;
    }
}

TEST(Trigonometry, SinCosReturnsNanForNonfiniteAngles)
{
    for (float angle : {INFINITY, -INFINITY, std::numeric_limits<float>::quiet_NaN()})
    {
        float sine, cosine;
        ncl_math::SinCos(angle, sine, cosine);

        EXPECT_TRUE(std::isnan(sine));
        EXPECT_TRUE(std::isnan(cosine));
    }
}

TEST(Trigonometry, SinCosMatchesLibmAroundEveryHalfPiMultipleInFastRange)
{
    for (int multiple = -5215; multiple <= 5215; ++multiple)
    {
        const float center = float(double(multiple) * (std::numbers::pi / 2.0));
        const std::array angles = {std::nextafter(center, -INFINITY), center, std::nextafter(center, INFINITY)};

        for (float angle : angles)
        {
            float sine, cosine;
            ncl_math::SinCos(angle, sine, cosine);

            ASSERT_FLOAT_EQ(sine, std::sin(angle)) << angle;
            ASSERT_FLOAT_EQ(cosine, std::cos(angle)) << angle;
        }
    }
}

#if defined(_MSC_VER) && defined(_M_IX86)
TEST(Trigonometry, SinCosPreservesControlModeAndAccuracy)
{
    unsigned int original;
    ASSERT_EQ(_controlfp_s(&original, 0, 0), 0);

    for (unsigned int precision : {_PC_24, _PC_53, _PC_64})
    {
        for (unsigned int rounding : {_RC_NEAR, _RC_DOWN, _RC_UP, _RC_CHOP})
        {
            unsigned int control;
            EXPECT_EQ(_controlfp_s(&control, precision | rounding, _MCW_PC | _MCW_RC), 0);

            for (int index = -4096; index <= 4096; ++index)
            {
                const float angle = float(index) * 1.75f;
                float sine, cosine;
                ncl_math::SinCos(angle, sine, cosine);

                EXPECT_NEAR(sine, std::sin(angle), 6e-8f);
                EXPECT_NEAR(cosine, std::cos(angle), 6e-8f);
            }

            EXPECT_EQ(_controlfp_s(&control, 0, 0), 0);
            EXPECT_EQ(control & (_MCW_PC | _MCW_RC), precision | rounding);
        }
    }

    unsigned int restored;
    EXPECT_EQ(_controlfp_s(&restored, original, _MCW_PC | _MCW_RC), 0);
}
#endif
