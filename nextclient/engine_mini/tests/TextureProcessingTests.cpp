#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "engine.h"
#include "graphics/texture_processing.h"

namespace
{
    // one channel per pixel is enough to follow where things go; the rest stay zero
    std::vector<uint8_t> Image(int width, int height, std::initializer_list<uint8_t> reds)
    {
        std::vector<uint8_t> pixels(width * height * 4, 0);
        size_t i = 0;
        for (uint8_t red : reds)
            pixels[(i++) * 4] = red;
        return pixels;
    }

    std::vector<uint8_t> Reds(const std::vector<uint8_t>& pixels, int count)
    {
        std::vector<uint8_t> reds;
        for (int i = 0; i < count; i++)
            reds.push_back(pixels[i * 4]);
        return reds;
    }
}

TEST(MipMapRGBA, AveragesTwoByTwoBlocks)
{
    auto pixels = Image(4, 2, {
        0, 4, 8, 12,
        4, 8, 12, 16,
    });
    std::vector<uint8_t> out(2 * 1 * 4);

    EXPECT_EQ(tex::MipMapRGBA(pixels.data(), 4, 2, out.data()), 1);
    EXPECT_EQ(Reds(out, 2), (std::vector<uint8_t>{4, 12}));
}

TEST(MipMapRGBA, OddWidthKeepsRowsAligned)
{
    // with the old loop the extra column bled into the next row and every row after it
    // started one pixel later, shearing the picture
    auto pixels = Image(5, 4, {
        0, 0, 100, 100, 200,
        0, 0, 100, 100, 200,
        40, 40, 80, 80, 200,
        40, 40, 80, 80, 200,
    });
    std::vector<uint8_t> out(2 * 2 * 4);

    EXPECT_EQ(tex::MipMapRGBA(pixels.data(), 5, 4, out.data()), 2);
    EXPECT_EQ(Reds(out, 4), (std::vector<uint8_t>{0, 100, 40, 80}));
}

TEST(MipMapRGBA, OneWideOrTallStillHalvesTheOtherSide)
{
    auto column = Image(1, 4, {0, 8, 16, 24});
    std::vector<uint8_t> out_column(1 * 2 * 4);
    EXPECT_EQ(tex::MipMapRGBA(column.data(), 1, 4, out_column.data()), 2);
    EXPECT_EQ(Reds(out_column, 2), (std::vector<uint8_t>{4, 20}));

    auto row = Image(4, 1, {0, 8, 16, 24});
    std::vector<uint8_t> out_row(2 * 1 * 4);
    EXPECT_EQ(tex::MipMapRGBA(row.data(), 4, 1, out_row.data()), 1);
    EXPECT_EQ(Reds(out_row, 2), (std::vector<uint8_t>{4, 20}));
}

TEST(MipMapRGBA, WorksInPlace)
{
    auto pixels = Image(3, 3, {
        0, 8, 50,
        8, 16, 50,
        90, 90, 90,
    });

    EXPECT_EQ(tex::MipMapRGBA(pixels.data(), 3, 3, pixels.data()), 1);
    EXPECT_EQ(Reds(pixels, 1), (std::vector<uint8_t>{8}));
}
