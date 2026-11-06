#pragma once

#include <ncl_gl/program.h>
#include <next_engine_mini/PerformanceMonitorInterface.h>

namespace performance
{
    inline ncl_gl::ProgramBuildCallbacks MakeProgramBuildCallbacks(PerformanceMonitorInterface* monitor)
    {
        if (!monitor)
        {
            return {};
        }

        ncl_gl::ProgramBuildCallbacks callbacks;
        callbacks.user = monitor;
        callbacks.Begin = [](void* user, ncl_gl::ProgramBuildStage stage) -> uint64_t {
            PerformanceMonitorInterface* service = static_cast<PerformanceMonitorInterface*>(user);
            bool compile = stage == ncl_gl::ProgramBuildStage::Compile;

            service->AddCounter(compile ? PerfCounterId::ShaderCompiles : PerfCounterId::ShaderLinks, 1);

            return service->BeginCpu(compile ? PerfStage::ShaderCompile : PerfStage::ShaderLink);
        };
        callbacks.End = [](void* user, uint64_t token, bool success) {
            PerformanceMonitorInterface* service = static_cast<PerformanceMonitorInterface*>(user);

            if (token)
            {
                service->EndCpu(token);
            }

            if (!success)
            {
                service->AddCounter(PerfCounterId::ShaderBuildFailures, 1);
            }
        };

        return callbacks;
    }
} // namespace performance
