#pragma once

#include <cstdint>
#include <cstdlib>

#ifndef USE_PROFILER
#define USE_PROFILER 0
#endif

#if USE_PROFILER
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

namespace ncl_perf
{
    class NclProfileScope
    {
    private:
        TracyCZoneCtx zone_{};

    public:
        explicit NclProfileScope(const ___tracy_source_location_data* location)
        {
            if (TracyIsStarted)
            {
                zone_ = ___tracy_emit_zone_begin(location, 1);
            }
        }

        ~NclProfileScope()
        {
            if (TracyIsStarted && zone_.active && tracy::GetProfiler().IsConnected())
            {
                ___tracy_emit_zone_end(zone_);
            }
        }

        NclProfileScope(const NclProfileScope&) = delete;
        NclProfileScope& operator=(const NclProfileScope&) = delete;
    };

    inline bool Profiling_IsStarted()
    {
        return TracyIsStarted;
    }

    inline bool Profiling_IsConnected()
    {
        return TracyIsStarted && tracy::GetProfiler().IsConnected();
    }

    inline void Profiling_Start()
    {
        if (!TracyIsStarted)
        {
#ifdef _WIN32
            // Eager driver symbol loading can keep the symbol worker inside DbgHelp during shutdown.
            if (!tracy::GetEnvVar("TRACY_NO_DBGHELP_INIT_LOAD"))
            {
                _putenv_s("TRACY_NO_DBGHELP_INIT_LOAD", "1");
            }
#endif
            tracy::StartupProfiler();
        }
    }

    inline void Profiling_Stop()
    {
        if (TracyIsStarted)
        {
            tracy::ShutdownProfiler();
        }
    }

    inline uintptr_t Profiling_RuntimeIdentity()
    {
        return TracyIsStarted ? reinterpret_cast<uintptr_t>(&tracy::GetProfiler()) : 0;
    }

} // namespace ncl_perf

#define NCL_PROFILE_NAMED_SCOPE(name)                                                          \
    static const ___tracy_source_location_data TracyConcat(g_NclProfileLocation, __LINE__) = { \
        name, __func__, __FILE__, uint32_t(__LINE__), 0                                        \
    };                                                                                         \
    ::ncl_perf::NclProfileScope TracyConcat(ncl_profile_scope, __LINE__)(&TracyConcat(g_NclProfileLocation, __LINE__));
#define NCL_PROFILE_SCOPE() NCL_PROFILE_NAMED_SCOPE(nullptr)
#define NCL_PROFILE_VALUE(name, value)                            \
    do                                                            \
    {                                                             \
        if (TracyIsStarted && tracy::GetProfiler().IsConnected()) \
        {                                                         \
            TracyPlot(name, double(value));                       \
        }                                                         \
    } while (0);
#define NCL_PROFILE_THREAD_NAME(name)                          \
    do                                                         \
    {                                                          \
        if (TracyIsStarted && !tracy::QueueMemory_IsStopped()) \
        {                                                      \
            tracy::SetThreadName(name);                        \
        }                                                      \
    } while (0);

#else
namespace ncl_perf
{
    inline bool Profiling_IsStarted()
    {
        return false;
    }
    inline bool Profiling_IsConnected()
    {
        return false;
    }
    inline void Profiling_Start() {}
    inline void Profiling_Stop() {}
    inline uintptr_t Profiling_RuntimeIdentity()
    {
        return 0;
    }
} // namespace ncl_perf

#define NCL_PROFILE_SCOPE()
#define NCL_PROFILE_NAMED_SCOPE(name)
#define NCL_PROFILE_VALUE(name, value)
#define NCL_PROFILE_THREAD_NAME(name)
#endif
