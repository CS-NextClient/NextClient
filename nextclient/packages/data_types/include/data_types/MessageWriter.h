#pragma once

#include <cstddef>
#include <cstdint>

// Writes the payload of a GoldSrc message into a buffer it does not own, in the byte layout MessageReader
// reads. A write that does not fit writes nothing and marks the writer overflowed, except WriteString
// (see it), which falls back to a single terminator byte when the full string does not fit.
class MessageWriter
{
    uint8_t* data_{};
    size_t size_{};
    size_t position_{};
    bool overflowed_{};

public:
    MessageWriter(void* data, size_t size);

    void WriteByte(uint8_t value);
    void WriteLong(int32_t value);
    void WriteString(const char* value);

    bool is_overflowed() const;
    size_t get_space_used() const;

private:
    // Room for the next `count` bytes, or null when fewer remain.
    uint8_t* Reserve(size_t count);
};
