#include "hud/radar/integration/Performance.h"

#include <next_engine_mini/performance_program_callbacks.h>

namespace radar::integration
{
    uint32_t Performance::get_map_generation() const
    {
        return monitor_ ? monitor_->map_generation() : 0;
    }

    void Performance::ReportWorker(const performance::PerfWorkerReport& report)
    {
        if (monitor_)
        {
            monitor_->ReportWorker(report);
        }
    }

    void Performance::AddCounter(performance::PerfCounterId counter, uint64_t value)
    {
        if (monitor_)
        {
            monitor_->AddCounter(counter, value);
        }
    }

    ncl_gl::ProgramBuildCallbacks Performance::ProgramCallbacks() const
    {
        return performance::MakeProgramBuildCallbacks(monitor_);
    }
} // namespace radar::integration
