#pragma once

namespace goldsrc
{
    inline constexpr int HLBSP_VERSION = 30;
    inline constexpr int Q1BSP_VERSION = 29;

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
