#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include <data_types/MessageReader.h>

TEST(MessageReader, ReadsLittleEndianIntegers)
{
    const std::vector<uint8_t> bytes = {0xFE, 0xFE, 0x34, 0x12, 0xFE, 0xFF, 0x78, 0x56, 0x34, 0x12};
    MessageReader reader(bytes.data(), bytes.size());

    EXPECT_EQ(reader.ReadChar(), -2);
    EXPECT_EQ(reader.ReadByte(), 254);
    EXPECT_EQ(reader.ReadShort(), 0x1234);
    EXPECT_EQ(reader.ReadWord(), -2); // signed, byte-for-byte the same as ReadShort: 0xFFFE as int16_t
    EXPECT_EQ(reader.ReadLong(), 0x12345678);
    EXPECT_FALSE(reader.is_overflowed());
}

TEST(MessageReader, ReadsAFloatBitForBit)
{
    const float expected = -1234.5f;
    uint8_t bytes[sizeof(float)];
    std::memcpy(bytes, &expected, sizeof(bytes));
    MessageReader reader(bytes, sizeof(bytes));

    EXPECT_EQ(reader.ReadFloat(), expected);
}

TEST(MessageReader, ScalesCoordinatesAndAngles)
{
    // Sixteen eighths of a unit, a quarter turn in 8 and in 16 bits, minus eight eighths.
    const std::vector<uint8_t> bytes = {0x10, 0x00, 0x40, 0x00, 0x40, 0xF8, 0xFF};
    MessageReader reader(bytes.data(), bytes.size());

    EXPECT_EQ(reader.ReadCoord(), 2.0f);
    EXPECT_EQ(reader.ReadAngle(), 90.0f);
    EXPECT_EQ(reader.ReadHiResAngle(), 90.0f);
    EXPECT_EQ(reader.ReadCoord(), -1.0f);
}

TEST(MessageReader, AStringEndsAtItsTerminatorAndTheNextReadFollowsIt)
{
    const std::vector<uint8_t> bytes = {'h', 'i', 0, 7};
    MessageReader reader(bytes.data(), bytes.size());

    EXPECT_EQ(reader.ReadString(), "hi");
    EXPECT_EQ(reader.ReadByte(), 7);
    EXPECT_FALSE(reader.is_overflowed());
}

TEST(MessageReader, AStringAlsoEndsAt0xFFWithoutOverflowing)
{
    // 0xFF is indistinguishable, as a signed char, from the stock reader's past-the-end sentinel (-1), so
    // a real 0xFF byte in the data ends a string exactly like 0x00: excluded, consumed, no overflow.
    const std::vector<uint8_t> bytes = {'h', 'i', 0xFF, 7};
    MessageReader reader(bytes.data(), bytes.size());

    EXPECT_EQ(reader.ReadString(), "hi");
    EXPECT_EQ(reader.ReadByte(), 7);
    EXPECT_FALSE(reader.is_overflowed());
}

TEST(MessageReader, AReadPastTheEndOverflowsAndReturnsMinusOne)
{
    const std::vector<uint8_t> bytes = {1, 2, 3};
    MessageReader reader(bytes.data(), bytes.size());

    EXPECT_EQ(reader.ReadLong(), -1);
    EXPECT_TRUE(reader.is_overflowed());
}

TEST(MessageReader, AStringWithoutATerminatorReadsToTheEndWithoutOverflowing)
{
    const std::vector<uint8_t> bytes = {'a', 'b'};
    MessageReader reader(bytes.data(), bytes.size());

    EXPECT_EQ(reader.ReadString(), "ab");
    EXPECT_FALSE(reader.is_overflowed());
}

TEST(MessageReader, AReadAfterAStringRanOffTheEndStillOverflowsNormally)
{
    const std::vector<uint8_t> bytes = {'a', 'b'};
    MessageReader reader(bytes.data(), bytes.size());

    reader.ReadString();

    EXPECT_EQ(reader.ReadByte(), -1);
    EXPECT_TRUE(reader.is_overflowed());
}
