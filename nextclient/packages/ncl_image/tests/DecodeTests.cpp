#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <ncl_image/Decode.h>

namespace
{
    // The header scan succeeds at IDAT, before reading its pixel data.
    constexpr std::size_t kPngDataOffset = 41;
    constexpr std::array<uint8_t, 77> kPng = {
        137, 80, 78, 71, 13, 10, 26, 10,
        0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 2, 0, 0, 0, 1, 8, 6, 0, 0, 0, 244, 34, 127, 138,
        0, 0, 0, 20, 73, 68, 65, 84, 120, 1, 1, 9, 0, 246, 255, 0, 255, 0, 0, 128, 0, 255, 64, 255,
        14, 253, 3, 190, 160, 254, 72, 86,
        0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
    };

    constexpr std::array<uint8_t, 8> kMalformed = {1, 2, 3, 4, 5, 6, 7, 8};

    std::vector<uint8_t> Tga()
    {
        return {
            0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 32, 40,
            0, 0, 255, 128, 64, 255, 0, 255
        };
    }

    void ExpectPixels(const ncl_image::RgbaImage& image)
    {
        ASSERT_EQ(image.width, 2);
        ASSERT_EQ(image.height, 1);
        ASSERT_NE(image.pixels.get(), nullptr);

        EXPECT_EQ(
            std::vector<uint8_t>(image.pixels.get(), image.pixels.get() + 8),
            std::vector<uint8_t>({255, 0, 0, 128, 0, 255, 64, 255})
        );
    }
}

TEST(ImageDecode, DecodesPngRgbaAndAlpha)
{
    auto result = ncl_image::DecodeRgba8(kPng, 2);
    ASSERT_FALSE(result.has_error()) << result.error_str();

    ExpectPixels(*result);
}

TEST(ImageDecode, DecodesTgaRgbaAndAlpha)
{
    auto result = ncl_image::DecodeRgba8(Tga(), 2);
    ASSERT_FALSE(result.has_error()) << result.error_str();

    ExpectPixels(*result);
}

TEST(ImageDecode, OwnsDecodedPixelsWhenInputChangesAndResultMoves)
{
    std::vector<uint8_t> encoded = Tga();
    auto result = ncl_image::DecodeRgba8(encoded, 2);
    ASSERT_FALSE(result.has_error()) << result.error_str();
    const uint8_t* pixels = result->pixels.get();
    std::fill(encoded.begin(), encoded.end(), 0);

    auto moved = std::move(result);
    ASSERT_FALSE(moved.has_error());
    EXPECT_EQ(moved->pixels.get(), pixels);
    EXPECT_EQ(result->pixels.get(), nullptr);
    ExpectPixels(*moved);
}

TEST(ImageDecode, RejectsEmptyAndMalformedInput)
{
    auto empty = ncl_image::DecodeRgba8({}, 2);
    ASSERT_TRUE(empty.has_error());
    EXPECT_EQ(empty.error().code, ncl_image::DecodeErrorCode::InputSize);

    auto invalid = ncl_image::DecodeRgba8(kMalformed, 2);
    ASSERT_TRUE(invalid.has_error());
    EXPECT_EQ(invalid.error().code, ncl_image::DecodeErrorCode::HeaderDimensions);
}

TEST(ImageDecode, RejectsTruncatedPngAfterReadingItsHeader)
{
    auto result = ncl_image::DecodeRgba8(std::span<const uint8_t>(kPng).first(kPngDataOffset), 2);
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code, ncl_image::DecodeErrorCode::Decoder);
    EXPECT_FALSE(result.error_str().empty());
}

TEST(ImageDecode, AppliesPixelBudgetToTheActualArea)
{
    for (uint64_t budget : {0ull, 1ull})
    {
        SCOPED_TRACE(budget);
        auto rejected = ncl_image::DecodeRgba8(Tga(), budget);
        ASSERT_TRUE(rejected.has_error());
        EXPECT_EQ(rejected.error().code, ncl_image::DecodeErrorCode::HeaderDimensions);
    }

    std::vector<uint8_t> strip = Tga();
    strip[12] = 1;
    strip[14] = 2;
    auto accepted = ncl_image::DecodeRgba8(strip, 2);
    ASSERT_FALSE(accepted.has_error()) << accepted.error_str();
    EXPECT_EQ(accepted->width, 1);
    EXPECT_EQ(accepted->height, 2);
}

TEST(ImageDecode, RejectsZeroDimensionsAndRgbaByteCountOverflowBeforeDecoding)
{
    std::vector<uint8_t> zero = Tga();
    zero[12] = 0;
    auto invalid = ncl_image::DecodeRgba8(zero, std::numeric_limits<uint64_t>::max());
    ASSERT_TRUE(invalid.has_error());
    EXPECT_EQ(invalid.error().code, ncl_image::DecodeErrorCode::HeaderDimensions);

    std::vector<uint8_t> oversized = Tga();
    oversized[12] = oversized[13] = oversized[14] = oversized[15] = 255;
    auto overflow = ncl_image::DecodeRgba8(oversized, std::numeric_limits<uint64_t>::max());
    ASSERT_TRUE(overflow.has_error());
    EXPECT_EQ(overflow.error().code, ncl_image::DecodeErrorCode::HeaderDimensions);
}
