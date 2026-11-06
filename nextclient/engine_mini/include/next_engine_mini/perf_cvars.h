#pragma once

#include <cvars/CvarDefinition.h>

namespace performance
{
    inline constexpr cvars::CvarDefinition kPerfCvarDefs[] = {
        {"cl_perf", "0", 0.0f, 3.0f},
        {"cl_perf_gpu", "1", 0.0f, 1.0f},
        {"cl_perf_scale", "1", 0.0f, 3.0f},
        {"cl_perf_net", "0", 0.0f, 1.0f},
        {"cl_perf_map", "0", 0.0f, 2.0f},
        {"cl_perf_batch", "0", 0.0f, 2.0f},
        {"cl_perf_background_alpha", "0.2", 0.0f, 1.0f}
    };
} // namespace performance
