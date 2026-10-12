#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>

namespace goldsrc
{
    using CRC32_t = unsigned int;
    using qboolean = int;
    using vec3_t = float[3];
    using vec_t = float;

    struct cache_user_t
    {
        void* data;
    };

    struct color24
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    struct colorVec
    {
        unsigned int r;
        unsigned int g;
        unsigned int b;
        unsigned int a;
    };

    struct vrect_t
    {
        int x;
        int y;
        int width;
        int height;
        vrect_t* pnext;
    };

    struct wrect_t
    {
        int left;
        int right;
        int top;
        int bottom;
    };
} // namespace goldsrc
