#pragma once

#include <next_engine_mini/PerformanceData.h>

namespace performance
{
    struct PerfMapperSnapshot;
    struct PerfNetworkSnapshot;

    namespace dump
    {
        uint32_t Format(
            const PerfSnapshot& snapshot,
            char* output,
            uint32_t capacity,
            const PerfNetworkSnapshot* network = nullptr,
            const PerfMapperSnapshot* mapper = nullptr
        );

        uint32_t FormatMapper(const PerfMapperSnapshot& mapper, char* output, uint32_t capacity);
    } // namespace dump
} // namespace performance
