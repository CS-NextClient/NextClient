#pragma once
#include <cstdint>
#include "common/sys_dll.h"

template <uint32_t BufferSize>
class ScratchAllocator
{
    uint8_t buffer_[BufferSize]{};
    size_t offset_{};

public:
    void* Allocate(size_t size)
    {
        // Align to 16 bytes
        size_t aligned_size = (size + 15) & ~15;

        if (offset_ + aligned_size > BufferSize)
        {
            Sys_Error("Scratch buffer overflow! Requested: %u, Available: %u", size, BufferSize - offset_);
        }

        void* ptr = (uint8_t*)(buffer_) + offset_;
        offset_ += aligned_size;
        return ptr;
    }

    void Reset(size_t to_offset = 0)
    {
        offset_ = to_offset;
    }

    size_t GetOffset() const
    {
        return offset_;
    }
};
