#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>

namespace goldsrc
{
    enum VidTypes : int
    {
        VT_None = 0,
        VT_Software = 1,
        VT_OpenGL = 2,
        VT_Direct3D = 3
    };

    struct alignas(4) transObjRef
    {
        cl_entity_t* pEnt;
        unsigned char unmodelled_[8 - (0 + sizeof(pEnt))];
    };

    struct alignas(4) viddef_t
    {
        uint8_t* buffer;
        uint8_t* colormap;
        unsigned short* colormap16;
        int fullbright;
        int bits;
        int is15bit;
        unsigned int rowbytes;
        unsigned int width;
        unsigned int height;
        float aspect;
        int numpages;
        int recalc_refdef;
        unsigned char unmodelled_[80 - (44 + sizeof(recalc_refdef))];
    };
} // namespace goldsrc
