#include <gtest/gtest.h>

// screenfade.h references `byte` but does not define it; provide the typedef before
// including the POD header. The fade math is engine-free, so no engine module links.
typedef unsigned char byte;
#include <screenfade.h>

#include "RadarFade.h"

namespace
{
    constexpr int kFadeOut = 0x0001; // FFADE_OUT
    constexpr int kFadeStayout = 0x0004; // FFADE_STAYOUT
} // namespace

TEST(RadarFadeAlpha, ExpiredFadeReturnsZero)
{
    screenfade_t sf = {};
    sf.fadeReset = 1.0f;
    sf.fadeEnd = 1.0f;
    sf.fadealpha = 255;
    sf.fadeFlags = kFadeOut;
    EXPECT_EQ(RadarFade::Alpha(sf, 2.0), 0);
}

TEST(RadarFadeAlpha, FadeOutPeakClampsToFadeAlpha)
{
    screenfade_t sf = {};
    sf.fadeSpeed = 100.0f;
    sf.fadeEnd = 3.0f;
    sf.fadealpha = 200;
    sf.fadeFlags = kFadeOut;
    // alpha = 100*(3-2) + 200 = 300, clamped to fadealpha (200).
    int a = RadarFade::Alpha(sf, 2.0);
    EXPECT_EQ(a, 200);
    EXPECT_GE(a, 150); // stays above the LocalBlind threshold at the flash peak
}

TEST(RadarFadeAlpha, FadeOutDecaysBelowThreshold)
{
    screenfade_t sf = {};
    sf.fadeSpeed = -100.0f;
    sf.fadeEnd = 3.0f;
    sf.fadealpha = 200;
    sf.fadeFlags = kFadeOut;
    // alpha = -100*(3-2) + 200 = 100.
    int a = RadarFade::Alpha(sf, 2.0);
    EXPECT_EQ(a, 100);
    EXPECT_LT(a, 150); // below the blind threshold as the flash clears
}

TEST(RadarFadeAlpha, StayoutHoldsAtFadeAlpha)
{
    screenfade_t sf = {};
    sf.fadealpha = 180;
    sf.fadeFlags = kFadeStayout;
    EXPECT_EQ(RadarFade::Alpha(sf, 5.0), 180);
}

TEST(RadarFadeNearWhite, TrueOnlyForWhiteFade)
{
    screenfade_t sf = {};
    sf.fader = 255;
    sf.fadeg = 255;
    sf.fadeb = 255;
    EXPECT_TRUE(RadarFade::NearWhite(sf));

    sf.fader = 200;
    sf.fadeg = 200;
    sf.fadeb = 200;
    EXPECT_TRUE(RadarFade::NearWhite(sf)); // boundary (>= 200)

    sf.fader = 255;
    sf.fadeg = 0;
    sf.fadeb = 0;
    EXPECT_FALSE(RadarFade::NearWhite(sf)); // tinted (red) map fade
}
