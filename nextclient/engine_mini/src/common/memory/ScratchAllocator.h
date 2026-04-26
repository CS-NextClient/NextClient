#pragma once
#include <cstdint>
#include "common/sys_dll.h"

class ScratchAllocator
{
    uint8_t* buffer_;
    size_t capacity_;
    size_t offset_{};

public:
    explicit ScratchAllocator(size_t capacity)
        : buffer_(new uint8_t[capacity]),
          capacity_(capacity)
    {
    }

    ~ScratchAllocator()
    {
        delete[] buffer_;
    }

    ScratchAllocator(const ScratchAllocator&) = delete;
    ScratchAllocator& operator=(const ScratchAllocator&) = delete;

    void* Allocate(size_t size)
    {
        // Align to 16 bytes
        size_t aligned_size = (size + 15) & ~15;

        if (offset_ + aligned_size > capacity_)
        {
            Sys_Error("Scratch buffer overflow! Requested: %u, Available: %u", size, capacity_ - offset_);
        }

        void* ptr = buffer_ + offset_;
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

    size_t GetCapacity() const
    {
        return capacity_;
    }
};