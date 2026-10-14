#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc
{
    inline constexpr int CONTENTS_LADDER = -16;
    inline constexpr int CONTENTS_SKY = -6;
    inline constexpr int CONTENTS_LAVA = -5;
    inline constexpr int CONTENTS_SLIME = -4;
    inline constexpr int CONTENTS_WATER = -3;
    inline constexpr int CONTENTS_SOLID = -2;
    inline constexpr int CONTENTS_EMPTY = -1;
    inline constexpr int HLBSP_VERSION = 30;
    inline constexpr int Q1BSP_VERSION = 29;

    struct dclipnode_t
    {
        int planenum;
        short children[2];
    };

    struct dmodel_t
    {
        float mins[3];
        float maxs[3];
        float origin[3];
        int headnode[4];
        int visleafs;
        int firstface;
        int numfaces;
    };
} // namespace goldsrc
