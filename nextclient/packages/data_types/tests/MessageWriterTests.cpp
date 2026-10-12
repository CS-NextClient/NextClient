#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <data_types/MessageWriter.h>

TEST(MessageWriter, WritesLittleEndianLongsBytesAndTerminatedStrings)
{
    std::vector<uint8_t> buffer(8, 0xCD);
    MessageWriter writer(buffer.data(), buffer.size());

    writer.WriteLong(0x12345678);
    writer.WriteByte(9);
    writer.WriteString("a");

    EXPECT_EQ(buffer, (std::vector<uint8_t>{0x78, 0x56, 0x34, 0x12, 9, 'a', 0, 0xCD}));
    EXPECT_EQ(writer.get_space_used(), 7u);
    EXPECT_FALSE(writer.is_overflowed());
}

TEST(MessageWriter, AWriteThatDoesNotFitWritesNothingAndOverflows)
{
    std::vector<uint8_t> buffer(3, 0xCD);
    MessageWriter writer(buffer.data(), buffer.size());

    writer.WriteLong(1);

    EXPECT_EQ(buffer, (std::vector<uint8_t>(3, 0xCD)));
    EXPECT_EQ(writer.get_space_used(), 0u);
    EXPECT_TRUE(writer.is_overflowed());
}

TEST(MessageWriter, AStringThatDoesNotFitFallsBackToASingleTerminatorByte)
{
    // The stock writer never leaves a partial, unterminated string in the buffer: when the real one (5
    // bytes: 4 characters and a terminator) does not fit, it writes a single 0x00 instead.
    std::vector<uint8_t> buffer(3, 0xCD);
    MessageWriter writer(buffer.data(), buffer.size());

    writer.WriteString("abcd");

    EXPECT_EQ(buffer, (std::vector<uint8_t>{0, 0xCD, 0xCD}));
    EXPECT_EQ(writer.get_space_used(), 1u);
    EXPECT_TRUE(writer.is_overflowed());
}

TEST(MessageWriter, AStringFallbackByteIsSkippedWhenNoRoomRemainsEither)
{
    std::vector<uint8_t> buffer(2, 0xCD);
    MessageWriter writer(buffer.data(), buffer.size());

    writer.WriteByte(1);
    writer.WriteByte(2);
    writer.WriteString("x");

    EXPECT_EQ(buffer, (std::vector<uint8_t>{1, 2}));
    EXPECT_EQ(writer.get_space_used(), 2u);
    EXPECT_TRUE(writer.is_overflowed());
}
