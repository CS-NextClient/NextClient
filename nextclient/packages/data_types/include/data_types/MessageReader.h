#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Reads the payload of a GoldSrc message from a buffer it does not own: little-endian integers, IEEE-754
// floats, strings terminated by 0x00 or 0xFF, coordinates in eighths of a unit, angles as fractions of a
// full turn. A read past the end marks the reader overflowed and returns -1, except ReadString (see it).
class MessageReader
{
    const uint8_t* data_{};
    size_t size_{};
    size_t position_{};
    bool overflowed_{};

public:
    MessageReader(const void* data, size_t size);

    int ReadChar();
    int ReadByte();
    int ReadShort();
    int ReadWord();
    int ReadLong();
    float ReadFloat();
    std::string ReadString();
    float ReadCoord();
    float ReadAngle();
    float ReadHiResAngle();

    size_t get_remaining_size() const;
    bool is_overflowed() const;

private:
    // The next `count` bytes, or null when fewer remain.
    const uint8_t* Take(size_t count);
};
