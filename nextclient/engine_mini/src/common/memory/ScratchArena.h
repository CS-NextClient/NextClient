#pragma once
#include "ScratchAllocator.h"

class ScratchArena
{
    size_t saved_offset_;
    ScratchAllocator& allocator_;

public:
    explicit ScratchArena(ScratchAllocator& allocator) :
        allocator_{allocator}
    {
        saved_offset_ = allocator.GetOffset();
    }

    ~ScratchArena()
    {
        allocator_.Reset(saved_offset_);
    }

    ScratchArena(const ScratchArena&) = delete;
    ScratchArena& operator=(const ScratchArena&) = delete;

    template <typename T>
    T* AllocateArray(size_t count)
    {
        return static_cast<T*>(allocator_.Allocate(count * sizeof(T)));
    }
};