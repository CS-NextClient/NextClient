#pragma once

#include <goldsrc/types.h>

namespace goldsrc
{
    struct dlight_t
    {
        vec3_t origin;
        float radius;
        color24 color;
        float die;
        float decay;
        float minlight;
        int key;
        qboolean dark;
    };
} // namespace goldsrc
