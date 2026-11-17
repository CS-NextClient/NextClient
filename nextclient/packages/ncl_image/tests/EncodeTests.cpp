#include <array>
#include <limits>
#include <vector>

#include <gtest/gtest.h>
#include <ncl_image/Decode.h>
#include <ncl_image/Encode.h>

TEST(ImageEncode, RoundTripsGrayscaleAsOpaqueRgba)
{
    const std::array<uint8_t, 4> pixels = {0, 71, 182, 255};
    auto encoded = ncl_image::EncodePng(2, 2, 1, pixels);
    ASSERT_FALSE(encoded.has_error()) << encoded.error_str();
    auto decoded = ncl_image::DecodeRgba8(*encoded, 4);
    ASSERT_FALSE(decoded.has_error()) << decoded.error_str();
    EXPECT_EQ(decoded->width, 2);
    EXPECT_EQ(decoded->height, 2);
    for (size_t index = 0; index < pixels.size(); index++)
    {
        for (size_t channel = 0; channel < 3; channel++)
        {
            EXPECT_EQ(decoded->pixels.get()[index * 4 + channel], pixels[index]);
        }
        EXPECT_EQ(decoded->pixels.get()[index * 4 + 3], 255);
    }
}

TEST(ImageEncode, RoundTripsRgbaIncludingAlphaAndIsDeterministic)
{
    const std::array<uint8_t, 8> pixels = {255, 0, 71, 0, 9, 27, 183, 128};
    auto encoded = ncl_image::EncodePng(2, 1, 4, pixels);
    ASSERT_FALSE(encoded.has_error()) << encoded.error_str();
    auto second = ncl_image::EncodePng(2, 1, 4, pixels);
    ASSERT_FALSE(second.has_error()) << second.error_str();
    EXPECT_EQ(*encoded, *second);
    auto decoded = ncl_image::DecodeRgba8(*encoded, 2);
    ASSERT_FALSE(decoded.has_error()) << decoded.error_str();
    EXPECT_EQ(std::vector<uint8_t>(decoded->pixels.get(), decoded->pixels.get() + 8), std::vector<uint8_t>(pixels.begin(), pixels.end()));
}

TEST(ImageEncode, RejectsInvalidDimensionsChannelsAndPixelExtent)
{
    const std::array<uint8_t, 4> pixels{};
    EXPECT_TRUE(ncl_image::EncodePng(0, 1, 4, pixels).has_error());
    EXPECT_TRUE(ncl_image::EncodePng(1, -1, 4, pixels).has_error());
    EXPECT_TRUE(ncl_image::EncodePng(1, 1, 3, pixels).has_error());
    EXPECT_TRUE(ncl_image::EncodePng(2, 1, 4, pixels).has_error());
    EXPECT_TRUE(ncl_image::EncodePng(std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), 4, pixels).has_error());
}
