#pragma once

#include <cstdint>

namespace goldsrc
{
    struct lumpinfo_t
    {
        int filepos;
        int disksize;
        int size;
        char type;
        char compression;
        char pad1;
        char pad2;
        char name[16];
    };

    struct qpic_t
    {
        int width;
        int height;
        uint8_t data[4];
    };

    struct wadinfo_t
    {
        char identification[4];
        int numlumps;
        int infotableofs;
    };
} // namespace goldsrc
