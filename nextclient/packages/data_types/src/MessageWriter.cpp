#include <data_types/MessageWriter.h>

#include <cstring>

MessageWriter::MessageWriter(void* data, size_t size) :
    data_(static_cast<uint8_t*>(data)),
    size_(size)
{
}

void MessageWriter::WriteByte(uint8_t value)
{
    uint8_t* room = Reserve(1);

    if (room)
    {
        room[0] = value;
    }
}

void MessageWriter::WriteLong(int32_t value)
{
    uint8_t* room = Reserve(4);

    if (!room)
    {
        return;
    }

    const uint32_t bits = static_cast<uint32_t>(value);

    room[0] = static_cast<uint8_t>(bits);
    room[1] = static_cast<uint8_t>(bits >> 8);
    room[2] = static_cast<uint8_t>(bits >> 16);
    room[3] = static_cast<uint8_t>(bits >> 24);
}

void MessageWriter::WriteString(const char* value)
{
    const size_t length = std::strlen(value) + 1;
    uint8_t* room = Reserve(length);

    if (room)
    {
        std::memcpy(room, value, length);
        return;
    }

    // The stock writer never leaves a partial, unterminated string in the buffer: when the real one does
    // not fit, it falls back to a single terminator byte, encoding an empty string, if that much still fits.
    uint8_t* fallback = Reserve(1);

    if (fallback)
    {
        fallback[0] = 0;
    }
}

bool MessageWriter::is_overflowed() const
{
    return overflowed_;
}

size_t MessageWriter::get_space_used() const
{
    return position_;
}

uint8_t* MessageWriter::Reserve(size_t count)
{
    if (size_ - position_ < count)
    {
        overflowed_ = true;
        return nullptr;
    }

    uint8_t* room = data_ + position_;
    position_ += count;

    return room;
}
