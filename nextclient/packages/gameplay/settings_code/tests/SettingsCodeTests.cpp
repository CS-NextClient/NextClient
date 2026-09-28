#include <gtest/gtest.h>

#include <cmath>

#include <settings_code/settings_code.h>

using namespace settings_code;

namespace
{
    Values SomeValues()
    {
        Values values{};
        values[kCrosshairType] = 3;
        values[kCrosshairSize] = 4;
        values[kCrosshairColorR] = 255;
        values[kCrosshairColorG] = 0;
        values[kCrosshairColorB] = 128;
        values[kCrosshairTranslucent] = 0;
        values[kDynamicCrosshair] = 1;
        values[kBobStyle] = 2;
        values[kBob] = 0.037f;
        values[kBobCycle] = 0.8f;
        values[kBobUp] = 0.95f;
        values[kBobAmtVert] = 0.13f;
        values[kBobAmtLat] = 0.32f;
        values[kBobLowerAmt] = 8;
        values[kViewmodelOffsetX] = -1.5f;
        values[kViewmodelOffsetY] = 7.99f;
        values[kViewmodelOffsetZ] = -8;
        values[kViewmodelFov] = 100;
        values[kViewmodelDisableShift] = 1;
        values[kLagStyle] = 1;
        values[kLagScale] = 4.25f;
        values[kLagSpeed] = 12.3f;
        values[kRollAngle] = 2;
        values[kRollSpeed] = 400;
        values[kCameraMovementScale] = 1.5f;
        values[kCameraMovementInterp] = 0.1f;
        values[kBobCamera] = 1;
        return values;
    }

    void ExpectNear(const Values& expected, const Values& actual, uint8_t sections)
    {
        for (int i = 0; i < kFieldCount; i++)
        {
            if ((sections >> kFields[i].section) & 1)
                EXPECT_NEAR(expected[i], actual[i], kFields[i].step / 2) << kFields[i].cvar << " (field " << i << ")";
        }
    }
}

TEST(SettingsCode, RoundTripsEverySection)
{
    Values values = SomeValues();

    std::optional<Decoded> decoded = Decode(Encode(values, kAllSections));

    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->sections, kAllSections);
    ExpectNear(values, decoded->values, kAllSections);
}

TEST(SettingsCode, RoundTripsSomeSections)
{
    Values values = SomeValues();
    uint8_t sections = (1 << kCrosshair) | (1 << kCamera);

    std::optional<Decoded> decoded = Decode(Encode(values, sections));

    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->sections, sections);
    ExpectNear(values, decoded->values, sections);
}

TEST(SettingsCode, IsShortAndStable)
{
    std::string code = Encode(SomeValues(), kAllSections);

    EXPECT_TRUE(code.starts_with("NCL-"));
    EXPECT_LE(code.size(), 40u);
    EXPECT_EQ(code, Encode(SomeValues(), kAllSections));
}

TEST(SettingsCode, ClampsValuesOutOfRange)
{
    Values values = SomeValues();
    values[kViewmodelFov] = 130;
    values[kViewmodelOffsetX] = -20;
    values[kBob] = NAN;

    std::optional<Decoded> decoded = Decode(Encode(values, kAllSections));

    ASSERT_TRUE(decoded);
    EXPECT_FLOAT_EQ(decoded->values[kViewmodelFov], 100);
    EXPECT_FLOAT_EQ(decoded->values[kViewmodelOffsetX], -8);
    EXPECT_FLOAT_EQ(decoded->values[kBob], 0);
}

TEST(SettingsCode, RejectsDamagedCodes)
{
    std::string code = Encode(SomeValues(), kAllSections);

    std::string flipped = code;
    flipped[10] = flipped[10] == 'A' ? 'B' : 'A';

    EXPECT_FALSE(Decode(flipped));
    EXPECT_FALSE(Decode(code.substr(0, code.size() - 3)));
    EXPECT_FALSE(Decode(code + "AAAA"));
    EXPECT_FALSE(Decode(code + "!"));
    EXPECT_FALSE(Decode(code.substr(4)));
    EXPECT_FALSE(Decode(""));
    EXPECT_FALSE(Decode("NCL-"));
    EXPECT_FALSE(Decode("hello"));
}

TEST(SettingsCode, RejectsOtherVersions)
{
    // version 2, no sections, then the CRC-8 of that byte pair: 0x20 0x00 0xAE
    EXPECT_FALSE(Decode("NCL-IACu"));
    EXPECT_TRUE(Decode(Encode(SomeValues(), 0)));
}
