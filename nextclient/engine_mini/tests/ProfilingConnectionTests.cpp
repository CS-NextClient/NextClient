#include <source_location>

#include <gtest/gtest.h>

#include <glad/glad.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#include <tracy/TracyOpenGL.hpp>

namespace
{
    uint64_t g_Connection = 1;
    uint32_t g_Ends{};
    const ___tracy_source_location_data* g_Location{};

    TracyCZoneCtx EmitBegin(const ___tracy_source_location_data* location, int)
    {
        g_Location = location;
        ++g_Connection;
        return {7, 1, g_Connection};
    }
    void EmitEnd(TracyCZoneCtx)
    {
        ++g_Ends;
    }
} // namespace

namespace tracy
{
    struct ConnectionTestProfiler
    {
        uint64_t ConnectionId() const
        {
            return g_Connection;
        }
        bool IsConnected() const
        {
            return true;
        }
    };
    ConnectionTestProfiler& ConnectionTestGetProfiler()
    {
        static ConnectionTestProfiler profiler;
        return profiler;
    }
} // namespace tracy

// Replace only the connection boundary and zone emission, keeping both production wrappers.
#define GetProfiler ConnectionTestGetProfiler
#define ___tracy_emit_zone_begin EmitBegin
#define ___tracy_emit_zone_end EmitEnd
#include <ncl_perf/profiler.h>
#include "performance/TracyGpuAdapter.cpp"
#undef GetProfiler
#undef ___tracy_emit_zone_begin
#undef ___tracy_emit_zone_end

TEST(ProfilingConnectionTest, ScopeClosesTheSessionThatBecameConnectedDuringBegin)
{
    ncl_perf::Profiling_Start();
    g_Ends = 0;
    static const ___tracy_source_location_data location = {"test", "test", __FILE__, __LINE__, 0};
    {
        ncl_perf::NclProfileScope scope(&location);
    }
    EXPECT_EQ(g_Ends, 1u);
    ncl_perf::Profiling_Stop();
}

TEST(ProfilingConnectionTest, BackendClosesTheSessionThatBecameConnectedDuringBegin)
{
    ncl_perf::Profiling_Start();
    g_Ends = 0;
    performance::TracyPerformanceBackend backend;
    ncl_perf::PerfScopeToken_t token = backend.BeginCpu(performance::PerfStage::Host);
    ASSERT_NE(token, 0u);
    backend.EndCpu(token);
    EXPECT_EQ(g_Ends, 1u);
    ncl_perf::Profiling_Stop();
}

TEST(ProfilingConnectionTest, ScopeDoesNotEmitAfterManualShutdown)
{
    ncl_perf::Profiling_Start();
    g_Ends = 0;
    static const ___tracy_source_location_data location = {"test", "test", __FILE__, __LINE__, 0};
    {
        ncl_perf::NclProfileScope scope(&location);
        ncl_perf::Profiling_Stop();
    }
    EXPECT_EQ(g_Ends, 0u);
}

TEST(ProfilingConnectionTest, BackendDoesNotEmitAfterManualShutdown)
{
    ncl_perf::Profiling_Start();
    g_Ends = 0;
    performance::TracyPerformanceBackend backend;
    ncl_perf::PerfScopeToken_t token = backend.BeginCpu(performance::PerfStage::Host);
    ASSERT_NE(token, 0u);
    ncl_perf::Profiling_Stop();
    backend.EndCpu(token);
    EXPECT_EQ(g_Ends, 0u);
}

TEST(ProfilingConnectionTest, BackendReportsCallerFileFunctionAndLine)
{
    ncl_perf::Profiling_Start();
    performance::TracyPerformanceBackend backend;
    const std::source_location caller = std::source_location::current();
    const uint32_t line = __LINE__ + 1;
    ncl_perf::PerfScopeToken_t token = backend.BeginCpu(performance::PerfStage::StudioGeometry);
    ASSERT_NE(token, 0u);
    ASSERT_NE(g_Location, nullptr);
    EXPECT_STREQ(g_Location->file, __FILE__);
    EXPECT_STREQ(g_Location->function, caller.function_name());
    EXPECT_EQ(g_Location->line, line);
    EXPECT_STREQ(g_Location->name, "Geometry preparation");
    backend.EndCpu(token);
    ncl_perf::Profiling_Stop();
}

TEST(ProfilingConnectionTest, SameStagePreservesDifferentCallSites)
{
    ncl_perf::Profiling_Start();
    performance::TracyPerformanceBackend backend;
    ncl_perf::PerfScopeToken_t first = backend.BeginCpu(performance::PerfStage::StudioGeometry);
    const ___tracy_source_location_data* first_location = g_Location;
    backend.EndCpu(first);
    ncl_perf::PerfScopeToken_t second = backend.BeginCpu(performance::PerfStage::StudioGeometry);
    const ___tracy_source_location_data* second_location = g_Location;
    backend.EndCpu(second);

    EXPECT_NE(first_location, second_location);
    EXPECT_NE(first_location->line, second_location->line);
    EXPECT_STREQ(first_location->file, __FILE__);
    EXPECT_STREQ(second_location->file, __FILE__);
    ncl_perf::Profiling_Stop();
}

TEST(ProfilingConnectionTest, RepeatedCallSiteReusesStableSourceMetadata)
{
    ncl_perf::Profiling_Start();
    performance::TracyPerformanceBackend backend;
    const ___tracy_source_location_data* first_location = nullptr;
    for (uint32_t iteration = 0; iteration < ncl_perf::kPerfScopeCapacity * 2; ++iteration)
    {
        ncl_perf::PerfScopeId_t stage = iteration % ncl_perf::kPerfScopeCapacity;
        ncl_perf::PerfScopeToken_t token = backend.BeginCpu(stage);
        ASSERT_NE(token, 0u);
        if (iteration == 0)
        {
            first_location = g_Location;
        }
        else if (iteration == ncl_perf::kPerfScopeCapacity)
        {
            EXPECT_EQ(g_Location, first_location);
        }
        EXPECT_STREQ(first_location->file, __FILE__);
        backend.EndCpu(token);
    }
    ncl_perf::Profiling_Stop();
}
