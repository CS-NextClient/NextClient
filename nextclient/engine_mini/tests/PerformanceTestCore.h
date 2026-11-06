#pragma once

#include <chrono>

#include "performance/PerformanceMonitor.h"
#include "performance/PerformanceMapper.h"

namespace performance_test
{
    class Clock final : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t Now() override
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    };

    struct Core
    {
        Clock clock;
        performance::PerformanceMonitor monitor{clock};
        performance::PerformanceMapper mapper;
        unsigned int studio_depth{};
    };
} // namespace performance_test
