#include <cstdint>

#include <ncl_perf/profiler.h>

#ifdef _WIN32
#define PERF_PROBE_EXPORT __declspec(dllexport)
#else
#define PERF_PROBE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" PERF_PROBE_EXPORT uintptr_t PerfTest_RuntimeIdentity()
{
    return ncl_perf::Profiling_RuntimeIdentity();
}

extern "C" PERF_PROBE_EXPORT TracyCZoneCtx PerfTest_BeginCpu()
{
    static const ___tracy_source_location_data location{"Probe CPU", __func__, __FILE__, __LINE__, 0};
    return ___tracy_emit_zone_begin(&location, 1);
}

extern "C" PERF_PROBE_EXPORT void PerfTest_EndCpu(TracyCZoneCtx context)
{
    ___tracy_emit_zone_end(context);
}

extern "C" PERF_PROBE_EXPORT tracy::QueueMemoryStatus PerfTest_QueueMemoryStatus()
{
    return tracy::QueueMemory_GetStatus();
}

extern "C" PERF_PROBE_EXPORT void* PerfTest_QueueAllocate(size_t size)
{
    return tracy::QueueMemory_Allocate(size);
}

extern "C" PERF_PROBE_EXPORT void PerfTest_QueueFree(void* allocation)
{
    tracy::QueueMemory_Free(allocation);
}
