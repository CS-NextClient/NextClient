#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include <glad/glad.h>
#include <ncl_gl/context.h>
#include <ncl_gl/loader.h>
#include <ncl_perf/profiler.h>
#if USE_PROFILER
#include <tracy/TracyOpenGL.hpp>
#endif

#include "RadarGlContext.h"
#include "performance/GpuQueryBackend.h"
#include "performance/GpuQueryCollector.h"
#include "performance/performance_stages.h"
#include "performance/TracyGpuAdapter.h"

TEST(GlLoader, ReloadUsesTheCurrentOffscreenContext)
{
    RadarGlContext context;
    std::string error;
    ASSERT_TRUE(context.Initialize(error)) << error;
    ASSERT_EQ(RadarGlContext::Current(), context.native_context());
    ASSERT_TRUE(ncl_gl::Reload());
    EXPECT_TRUE(ncl_gl::IsLoaded());
    EXPECT_TRUE(GLAD_GL_VERSION_3_3);
    EXPECT_NE(ncl_gl::LoadProc("glGetString"), nullptr);
    EXPECT_EQ(RadarGlContext::Current(), context.native_context());
}

TEST(GlContext, NativeIdentityTracksReplacementAndClear)
{
    RadarGlContext first;
    std::string error;
    ASSERT_TRUE(first.Initialize(error)) << error;
    EXPECT_EQ(ncl_gl::CurrentContext(), first.native_context());

    RadarGlContext second;
    ASSERT_TRUE(second.Initialize(error)) << error;
    EXPECT_NE(first.native_context(), second.native_context());
    EXPECT_EQ(ncl_gl::CurrentContext(), second.native_context());

    ASSERT_TRUE(first.MakeCurrent());
    EXPECT_EQ(ncl_gl::CurrentContext(), first.native_context());
    ASSERT_TRUE(first.ClearCurrent());
    EXPECT_EQ(ncl_gl::CurrentContext(), nullptr);
    ASSERT_TRUE(first.MakeCurrent());
}

namespace
{
    constexpr GLuint kTimestampQueryName = 37;
    constexpr GLuint64 kTimestampValue = 0x12345678ABCDEF01ull;

    void APIENTRY ReadTimestamp64(GLuint name, GLenum parameter, GLuint64* value)
    {
        EXPECT_EQ(name, kTimestampQueryName);
        EXPECT_EQ(parameter, GLenum(GL_QUERY_RESULT));
        *value = kTimestampValue;
    }

    void APIENTRY ReadTimestamp32(GLuint name, GLenum parameter, GLuint* value)
    {
        EXPECT_EQ(name, kTimestampQueryName);
        EXPECT_EQ(parameter, GLenum(GL_QUERY_RESULT));
        *value = GLuint(kTimestampValue);
    }

    class TimestampReadFunctions
    {
    private:
        PFNGLGETQUERYOBJECTUI64VPROC read_64_ = glad_glGetQueryObjectui64v;
        PFNGLGETQUERYOBJECTUIVPROC read_32_ = glad_glGetQueryObjectuiv;

    public:
        TimestampReadFunctions()
        {
            glad_glGetQueryObjectui64v = ReadTimestamp64;
            glad_glGetQueryObjectuiv = ReadTimestamp32;
        }

        ~TimestampReadFunctions()
        {
            glad_glGetQueryObjectui64v = read_64_;
            glad_glGetQueryObjectuiv = read_32_;
        }
    };
} // namespace

TEST(PerformanceGpuGlTest, BackendTimestampReadPreservesAll64Bits)
{
    TimestampReadFunctions reads;
    performance::GpuQueryBackend backend;

    EXPECT_EQ(backend.Read(kTimestampQueryName), kTimestampValue);
}

TEST(PerformanceGpuGlTest, CompletedTimestampPairIsCollectedWithoutWaitingForAnUnavailableResult)
{
    RadarGlContext context;
    std::string error;
    ASSERT_TRUE(context.Initialize(error)) << error;
    performance::GpuQueryBackend backend;
    backend.Probe();
    if (!backend.IsTimestampSupported() || backend.counter_bits() < 64)
    {
        GTEST_SKIP() << "64-bit GL timestamp queries unavailable; bits=" << backend.counter_bits();
    }
    EXPECT_EQ(backend.counter_bits(), 64);
    performance::GpuQueryCollector collector(backend, 1, 1);
    ncl_perf::PerfScopeToken_t token = collector.Begin(7, 9, performance::PerfStage::StudioShader);
    ASSERT_NE(token, 0u);
    glClearColor(0.2f, 0.3f, 0.4f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    collector.End(token);
    glFlush();
    performance::PerfGpuResult result;
    uint32_t count = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!count && std::chrono::steady_clock::now() < deadline)
    {
        count = collector.Poll(&result, 1);
        if (!count)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    collector.ReleaseGl();
    ASSERT_EQ(count, 1u) << "Timestamp result did not become available within two seconds";
    EXPECT_EQ(result.state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(result.frame_id, 7u);
    EXPECT_EQ(result.output_id, 9u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

#if USE_PROFILER
namespace
{
    class ProfilingSession
    {
    public:
        ProfilingSession()
        {
            ncl_perf::Profiling_Start();
        }
        ~ProfilingSession()
        {
            ncl_perf::Profiling_Stop();
        }
    };
} // namespace

TEST(PerformanceGpuGlTest, NativeDiagnosticContextReleasesWithoutAViewer)
{
    RadarGlContext context;
    std::string error;
    ASSERT_TRUE(context.Initialize(error)) << error;
    performance::GpuQueryBackend backend;
    backend.Probe();
    if (!backend.IsTimestampSupported() || backend.counter_bits() <= 0)
    {
        GTEST_SKIP() << "GL timestamp queries unavailable";
    }
    ProfilingSession profiling;
    performance::TracyGpuAdapter adapter;
    adapter.Initialize();
    tracy::GpuCtx* first = tracy::GetGpuCtx().ptr;
    ASSERT_NE(first, nullptr);
    adapter.Initialize();
    EXPECT_EQ(tracy::GetGpuCtx().ptr, first);
    adapter.Collect();
    adapter.ReleaseGl();
    EXPECT_EQ(tracy::GetGpuCtx().ptr, nullptr);
    adapter.ReleaseGl();
    EXPECT_EQ(tracy::GetGpuCtx().ptr, nullptr);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
#endif
