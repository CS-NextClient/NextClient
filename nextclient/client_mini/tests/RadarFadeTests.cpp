#include <gtest/gtest.h>

#include <goldsrc/view.h>

#include "hud/radar/render/RadarFade.h"

namespace
{
    constexpr int kFadeOut = 0x0001; // FFADE_OUT
    constexpr int kFadeStayout = 0x0004; // FFADE_STAYOUT
} // namespace

TEST(RadarFadeAlpha, ExpiredFadeReturnsZero)
{
    goldsrc::screenfade_t sf = {};
    sf.fadeReset = 1.0f;
    sf.fadeEnd = 1.0f;
    sf.fadealpha = 255;
    sf.fadeFlags = kFadeOut;
    EXPECT_EQ(RadarFade_ComputeAlpha(sf, 2.0), 0);
}

TEST(RadarFadeAlpha, FadeOutPeakClampsToFadeAlpha)
{
    goldsrc::screenfade_t sf = {};
    sf.fadeSpeed = 100.0f;
    sf.fadeEnd = 3.0f;
    sf.fadealpha = 200;
    sf.fadeFlags = kFadeOut;
    int a = RadarFade_ComputeAlpha(sf, 2.0);
    EXPECT_EQ(a, 200);
    EXPECT_GE(a, 150); // above the IsLocalBlind threshold
}

TEST(RadarFadeAlpha, FadeOutDecaysBelowThreshold)
{
    goldsrc::screenfade_t sf = {};
    sf.fadeSpeed = -100.0f;
    sf.fadeEnd = 3.0f;
    sf.fadealpha = 200;
    sf.fadeFlags = kFadeOut;
    int a = RadarFade_ComputeAlpha(sf, 2.0);
    EXPECT_EQ(a, 100);
    EXPECT_LT(a, 150); // below the IsLocalBlind threshold
}

TEST(RadarFadeAlpha, StayoutHoldsAtFadeAlpha)
{
    goldsrc::screenfade_t sf = {};
    sf.fadealpha = 180;
    sf.fadeFlags = kFadeStayout;
    EXPECT_EQ(RadarFade_ComputeAlpha(sf, 5.0), 180);
}

TEST(RadarFadeNearWhite, TrueOnlyForWhiteFade)
{
    goldsrc::screenfade_t sf = {};
    sf.fader = 255;
    sf.fadeg = 255;
    sf.fadeb = 255;
    EXPECT_TRUE(RadarFade_IsNearWhite(sf));

    sf.fader = 200;
    sf.fadeg = 200;
    sf.fadeb = 200;
    EXPECT_TRUE(RadarFade_IsNearWhite(sf)); // boundary (>= 200)

    sf.fader = 255;
    sf.fadeg = 0;
    sf.fadeb = 0;
    EXPECT_FALSE(RadarFade_IsNearWhite(sf));
}
