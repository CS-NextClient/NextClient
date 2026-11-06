#include <data_types/MessageReader.h>

#include <bit>

namespace
{
    constexpr float kCoordScale = 1.0f / 8.0f;
    constexpr float kAngleScale = 360.0f / 256.0f;
    constexpr float kHiResAngleScale = 360.0f / 65536.0f;
    constexpr int kPastEnd = -1;

    uint32_t LittleEndian32(const uint8_t* bytes)
    {
        return bytes[0] | bytes[1] << 8 | bytes[2] << 16 | static_cast<uint32_t>(bytes[3]) << 24;
    }
} // namespace

MessageReader::MessageReader(const void* data, size_t size) :
    data_(static_cast<const uint8_t*>(data)),
    size_(size)
{
}

int MessageReader::ReadChar()
{
    const uint8_t* bytes = Take(1);

    if (!bytes)
    {
        return kPastEnd;
    }

    return static_cast<int8_t>(bytes[0]);
}

int MessageReader::ReadByte()
{
    const uint8_t* bytes = Take(1);

    if (!bytes)
    {
        return kPastEnd;
    }

    return bytes[0];
}

int MessageReader::ReadShort()
{
    const uint8_t* bytes = Take(2);

    if (!bytes)
    {
        return kPastEnd;
    }

    return static_cast<int16_t>(bytes[0] | bytes[1] << 8);
}

int MessageReader::ReadWord()
{
    // The stock reader's WORD read is signed, byte-for-byte the same as its SHORT read: it sign-extends
    // rather than zero-extending, so the upper half of the 16-bit range comes back negative.
    const uint8_t* bytes = Take(2);

    if (!bytes)
    {
        return kPastEnd;
    }

    return static_cast<int16_t>(bytes[0] | bytes[1] << 8);
}

int MessageReader::ReadLong()
{
    const uint8_t* bytes = Take(4);

    if (!bytes)
    {
        return kPastEnd;
    }

    return static_cast<int32_t>(LittleEndian32(bytes));
}

float MessageReader::ReadFloat()
{
    const uint8_t* bytes = Take(4);

    if (!bytes)
    {
        return kPastEnd;
    }

    return std::bit_cast<float>(LittleEndian32(bytes));
}

// The stock reader never marks the message overflowed here, even when it runs off the end still hunting
// for a terminator: unlike every other Read*, this one does not go through Take() and so does not affect
// is_overflowed(); a later read that genuinely has no bytes left still overflows normally.
std::string MessageReader::ReadString()
{
    std::string value;

    while (position_ < size_)
    {
        const uint8_t byte = data_[position_];
        ++position_;

        // The stock reader ends a string on 0x00 or on 0xFF: MSG_ReadByte's past-the-end sentinel is -1,
        // indistinguishable in a signed char from an in-bounds 0xFF byte, so a real 0xFF in the data ends
        // the string exactly like a genuine terminator would.
        if (byte == 0 || byte == 0xFF)
        {
            break;
        }

        value.push_back(static_cast<char>(byte));
    }

    return value;
}

float MessageReader::ReadCoord()
{
    return static_cast<float>(ReadShort()) * kCoordScale;
}

float MessageReader::ReadAngle()
{
    return static_cast<float>(ReadChar()) * kAngleScale;
}

float MessageReader::ReadHiResAngle()
{
    return static_cast<float>(ReadShort()) * kHiResAngleScale;
}

size_t MessageReader::get_remaining_size() const
{
    return size_ - position_;
}

bool MessageReader::is_overflowed() const
{
    return overflowed_;
}

const uint8_t* MessageReader::Take(size_t count)
{
    if (size_ - position_ < count)
    {
        overflowed_ = true;
        return nullptr;
    }

    const uint8_t* bytes = data_ + position_;
    position_ += count;

    return bytes;
}
