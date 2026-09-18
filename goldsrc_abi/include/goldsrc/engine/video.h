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

    struct transObjRef
    {
        transObjRef() = delete;
        transObjRef(const transObjRef&) = delete;
        transObjRef& operator=(const transObjRef&) = delete;

        cl_entity_t* pEnt;
    };

    struct viddef_t
    {
        viddef_t() = delete;
        viddef_t(const viddef_t&) = delete;
        viddef_t& operator=(const viddef_t&) = delete;

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
    };
} // namespace goldsrc
