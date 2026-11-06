#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <array>
#include <vector>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include <glad/glad.h>
#include <ncl_perf/profiler.h>
#include <nitro_utils/platform.h>
#include <tracy/TracyOpenGL.hpp>

#include "performance/TracyGpuAdapter.h"

namespace
{
    uint32_t g_DeletedQueries{};
    uint32_t g_ResultReads{};
    uint32_t g_AvailabilityReads{};
    uint32_t g_NoWaitReads{};
    bool g_QueryReady{};
    bool g_QueryBufferObject{};
    constexpr size_t kTracyQueueMemoryLimit = 128u * 1024u * 1024u;
    uint64_t g_Timestamp{};
    std::array<uint64_t, 65537> g_QueryTimes{};
    std::vector<uint64_t> g_ReadTimes;
    void APIENTRY GenerateQueries(GLsizei count, GLuint* queries)
    {
        for (GLsizei index = 0; index < count; ++index)
        {
            queries[index] = GLuint(index + 1);
        }
    }
    void APIENTRY DeleteQueries(GLsizei count, const GLuint*)
    {
        g_DeletedQueries += count;
    }
    void APIENTRY GetTimestamp(GLenum, GLint64* value)
    {
        *value = 123;
    }
    void APIENTRY GetQueryBits(GLenum, GLenum, GLint* value)
    {
        *value = 64;
    }
    void APIENTRY WriteTimestamp(GLuint query, GLenum)
    {
        g_QueryTimes[query] = g_Timestamp;
    }
    void APIENTRY GetQueryAvailable(GLuint, GLenum, GLint* value)
    {
        ++g_AvailabilityReads;
        *value = g_QueryReady;
    }
    void APIENTRY GetInteger(GLenum name, GLint* value)
    {
        *value = name == GL_MAJOR_VERSION ? 3 : (g_QueryBufferObject ? 2 : 1);
    }
    GLenum APIENTRY GetError()
    {
        return GL_NO_ERROR;
    }
    const GLubyte* APIENTRY GetString(GLenum)
    {
        return reinterpret_cast<const GLubyte*>("GL_ARB_timer_query");
    }
    const GLubyte* APIENTRY GetStringIndexed(GLenum, GLuint index)
    {
        return reinterpret_cast<const GLubyte*>(index == 0 ? "GL_ARB_timer_query" : "GL_ARB_query_buffer_object");
    }
    void APIENTRY ReadQuery(GLuint query, GLenum mode, GLuint64* value)
    {
        if (mode == GL_QUERY_RESULT_NO_WAIT)
        {
            ++g_NoWaitReads;
            if (!g_QueryReady)
            {
                return;
            }
        }
        ++g_ResultReads;
        *value = g_QueryTimes[query];
        g_ReadTimes.push_back(*value);
    }

    class DiagnosticGpuTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            g_DeletedQueries = g_ResultReads = g_AvailabilityReads = 0;
            g_NoWaitReads = 0;
            g_QueryReady = false;
            g_QueryBufferObject = false;
            g_Timestamp = 0;
            g_ReadTimes.clear();
            GLAD_GL_ARB_timer_query = 1;
            GLAD_GL_VERSION_3_0 = 1;
            glad_glGetIntegerv = GetInteger;
            glad_glGetError = GetError;
            glad_glGetString = GetString;
            glad_glGetStringi = GetStringIndexed;
            glad_glGenQueries = GenerateQueries;
            glad_glDeleteQueries = DeleteQueries;
            glad_glGetInteger64v = GetTimestamp;
            glad_glGetQueryiv = GetQueryBits;
            glad_glQueryCounter = WriteTimestamp;
            glad_glGetQueryObjectiv = GetQueryAvailable;
            glad_glGetQueryObjectui64v = ReadQuery;
            ncl_perf::Profiling_Start();
        }
        void TearDown() override
        {
            ncl_perf::Profiling_Stop();
        }
    };
} // namespace

#ifdef _WIN32
TEST(DiagnosticProfiling, RepeatedLifetimesReleaseVirtualAddressSpace)
{
    const auto reserved_bytes = []() {
        size_t total = 0;
        uintptr_t address = 0;
        MEMORY_BASIC_INFORMATION region{};

        while (VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)))
        {
            if (region.State != MEM_FREE)
            {
                total += region.RegionSize;
            }

            const uintptr_t next = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
            if (next <= address)
            {
                break;
            }
            address = next;
        }

        return total;
    };

    ncl_perf::Profiling_Start();
    NCL_PROFILE_THREAD_NAME("Diagnostic lifetime test");
    ncl_perf::Profiling_Stop();
    const size_t initial = reserved_bytes();

    for (int iteration = 0; iteration < 8; ++iteration)
    {
        ncl_perf::Profiling_Start();
        NCL_PROFILE_THREAD_NAME("Diagnostic lifetime test");
        ncl_perf::Profiling_Stop();
    }

    EXPECT_LT(reserved_bytes(), initial + 64u * 1024u * 1024u);
}
#endif

TEST_F(DiagnosticGpuTest, CompletePairsPreserveTheParentEndUnderSaturation)
{
    std::unique_ptr<tracy::GpuCtx> context = std::make_unique<tracy::GpuCtx>();
    unsigned int parent_start = 0;
    unsigned int parent_end = 0;
    ASSERT_TRUE(context->ReserveQueryPair(parent_start, parent_end));
    EXPECT_EQ(parent_end, parent_start + 1);
    context->WriteTimestamp(parent_start);
    unsigned int start = 0;
    unsigned int end = 0;
    uint32_t nested = 0;
    while (context->ReserveQueryPair(start, end))
    {
        context->WriteTimestamp(start);
        context->WriteTimestamp(end);
        ++nested;
    }
    EXPECT_EQ(nested, 32766u);
    g_QueryReady = true;
    context->Collect();
    EXPECT_EQ(g_ResultReads, 65533u);
    context->WriteTimestamp(parent_end);
    context->Collect();
    EXPECT_EQ(g_ResultReads, 65534u);
    EXPECT_TRUE(context->ReserveQueryPair(start, end));
    context->ReleaseGl();
    context->ReleaseGl();
    EXPECT_EQ(g_DeletedQueries, 65536u);
}

TEST_F(DiagnosticGpuTest, UndrainedEventQueueRejectsFurtherEventsWithinTheSharedBudget)
{
    tracy::moodycamel::ConcurrentQueue<tracy::QueueItem> queue(0);
    tracy::moodycamel::ProducerToken token(queue);
    auto* producer = queue.get_explicit_producer(token);
    const size_t attempts = kTracyQueueMemoryLimit / sizeof(tracy::QueueItem) + 65536u;
    bool rejected = false;

    for (size_t index = 0; index < attempts; ++index)
    {
        tracy::moodycamel::ConcurrentQueueDefaultTraits::index_t position;
        tracy::QueueItem* item = producer ? producer->enqueue_begin(position) : nullptr;
        if (!item)
        {
            rejected = true;
            break;
        }

        item->hdr.type = tracy::QueueType::KeepAlive;
        producer->get_tail_index().store(position + 1, std::memory_order_release);
    }

    EXPECT_TRUE(rejected);
    const tracy::QueueMemoryStatus status = tracy::QueueMemory_GetStatus();
    EXPECT_EQ(status.limit, kTracyQueueMemoryLimit);
    EXPECT_LE(status.used, status.limit);
    EXPECT_LE(status.peak, status.limit);
    EXPECT_EQ(status.stopped, 1u);
    EXPECT_TRUE(ncl_perf::Profiling_IsStarted());
    EXPECT_FALSE(ncl_perf::Profiling_IsConnected());
}

TEST_F(DiagnosticGpuTest, DrainingEventQueueContinuesBeyondTheCumulativeBudget)
{
    tracy::moodycamel::ConcurrentQueue<tracy::QueueItem> queue(0);
    tracy::moodycamel::ProducerToken producer_token(queue);
    tracy::moodycamel::ConsumerToken consumer_token(queue);
    auto* producer = queue.get_explicit_producer(producer_token);
    ASSERT_NE(producer, nullptr);
    size_t received = 0;

    for (size_t batch = 0; batch < 2048; ++batch)
    {
        for (size_t index = 0; index < 4096; ++index)
        {
            tracy::moodycamel::ConcurrentQueueDefaultTraits::index_t position;
            tracy::QueueItem* item = producer->enqueue_begin(position);
            ASSERT_NE(item, nullptr);
            item->hdr.type = tracy::QueueType::KeepAlive;
            producer->get_tail_index().store(position + 1, std::memory_order_release);
        }

        while (queue.try_dequeue_bulk_single(
            consumer_token,
            [](uint64_t) {},
            [&](tracy::QueueItem* items, size_t count) {
                for (size_t index = 0; index < count; ++index)
                {
                    EXPECT_EQ(items[index].hdr.type, tracy::QueueType::KeepAlive);
                }
                received += count;
            }
        ))
        {
        }
    }

    EXPECT_EQ(received, 2048u * 4096u);
    EXPECT_EQ(tracy::QueueMemory_GetStatus().stopped, 0u);
}

TEST_F(DiagnosticGpuTest, ConcurrentAllocationsShareTheHardLimitAndLeaveTheRuntimeAlive)
{
    std::array<std::array<void*, 32>, 8> allocations{};
    std::array<std::thread, 8> workers;
    const size_t initial = tracy::QueueMemory_GetStatus().used;

    for (size_t worker = 0; worker < workers.size(); ++worker)
    {
        workers[worker] = std::thread([&, worker]() {
            for (void*& allocation : allocations[worker])
            {
                allocation = tracy::QueueMemory_Allocate(1024u * 1024u);
                if (!allocation)
                {
                    break;
                }
            }
        });
    }
    for (std::thread& worker : workers)
    {
        worker.join();
    }

    const tracy::QueueMemoryStatus status = tracy::QueueMemory_GetStatus();
    EXPECT_LE(status.used, kTracyQueueMemoryLimit);
    EXPECT_LE(status.peak, kTracyQueueMemoryLimit);
    EXPECT_EQ(status.stopped, 1u);
    EXPECT_TRUE(ncl_perf::Profiling_IsStarted());
    EXPECT_FALSE(ncl_perf::Profiling_IsConnected());
    EXPECT_EQ(tracy::QueueMemory_Allocate(1), nullptr);
    {
        NCL_PROFILE_SCOPE();
        NCL_PROFILE_THREAD_NAME("capture stopped");
        NCL_PROFILE_VALUE("capture stopped", 1);
        TracyAppInfo("capture stopped", 15);
        tracy::Profiler::SectionSetup(1, "%s", "capture stopped");
    }

    for (const auto& worker : allocations)
    {
        for (void* allocation : worker)
        {
            tracy::QueueMemory_Free(allocation);
        }
    }
    EXPECT_EQ(tracy::QueueMemory_GetStatus().used, initial);
    EXPECT_EQ(tracy::QueueMemory_GetStatus().stopped, 1u);
}

TEST_F(DiagnosticGpuTest, SerialPayloadsAreReleasedWhileRecordingBeyondTheCumulativeBudget)
{
    const size_t initial = tracy::QueueMemory_GetStatus().used;
    {
        tracy::QueueVector<tracy::QueueItem> queue(64);
        for (size_t batch = 0; batch < 512; ++batch)
        {
            for (size_t index = 0; index < 64; ++index)
            {
                char* payload = static_cast<char*>(tracy::QueuePayload_Allocate(8192));
                ASSERT_NE(payload, nullptr);
                payload[0] = char(index);
                payload[8191] = char(batch);
                tracy::QueueItem* item = queue.prepare_next();
                item->hdr.type = tracy::QueueType::ZoneText;
                tracy::MemWrite(&item->zoneTextFat.text, uint64_t(payload));
                tracy::MemWrite(&item->zoneTextFat.size, uint16_t(8192));
                queue.commit_next();
            }

            ASSERT_EQ(queue.size(), 64u);
            for (size_t index = 0; index < queue.size(); ++index)
            {
                const char* payload = reinterpret_cast<const char*>(tracy::MemRead(&queue[index].zoneTextFat.text));
                EXPECT_EQ(payload[0], char(index));
                EXPECT_EQ(payload[8191], char(batch));
                tracy::QueueMemory_DiscardItem(queue[index]);
            }
            queue.clear();
        }
        EXPECT_EQ(tracy::QueueMemory_GetStatus().stopped, 0u);
        EXPECT_LT(tracy::QueueMemory_GetStatus().peak, 4u * 1024u * 1024u);
    }
    EXPECT_EQ(tracy::QueueMemory_GetStatus().used, initial);
}

TEST_F(DiagnosticGpuTest, DeferredMetadataSurvivesCaptureStopAndIsReleasedAtShutdown)
{
    TracyAppInfo("queue budget test", 17);
    tracy::Profiler::SectionSetup(1, "%s", "queue budget test");
    EXPECT_EQ(tracy::QueueMemory_GetStatus().stopped, 0u);
    EXPECT_EQ(tracy::QueueMemory_Allocate(kTracyQueueMemoryLimit), nullptr);
    EXPECT_EQ(tracy::QueueMemory_GetStatus().stopped, 1u);

    ncl_perf::Profiling_Stop();
    EXPECT_EQ(tracy::QueueMemory_GetStatus().used, 0u);
}

TEST_F(DiagnosticGpuTest, CollectionChecksAvailabilityAndLostContextDoesNotDelete)
{
    std::unique_ptr<tracy::GpuCtx> context = std::make_unique<tracy::GpuCtx>();
    unsigned int start = 0;
    unsigned int end = 0;
    ASSERT_TRUE(context->ReserveQueryPair(start, end));
    context->WriteTimestamp(start);
    context->WriteTimestamp(end);
    context->Collect();
    EXPECT_EQ(g_AvailabilityReads, 1u);
    EXPECT_EQ(g_ResultReads, 0u);
    context->AbandonGl();
    context->ReleaseGl();
    EXPECT_EQ(g_DeletedQueries, 0u);
}

TEST_F(DiagnosticGpuTest, AdapterOwnsOneContextAndSupportsLiveAndLostCleanup)
{
    performance::TracyGpuAdapter adapter;
    adapter.Initialize();
    tracy::GpuCtx* first = tracy::GetGpuCtx().ptr;
    ASSERT_NE(first, nullptr);
    adapter.Initialize();
    EXPECT_EQ(first, tracy::GetGpuCtx().ptr);
    adapter.ReleaseGl();
    EXPECT_EQ(tracy::GetGpuCtx().ptr, nullptr);
    EXPECT_EQ(g_DeletedQueries, 65536u);
    adapter.ReleaseGl();
    EXPECT_EQ(g_DeletedQueries, 65536u);
    adapter.Initialize();
    ASSERT_NE(tracy::GetGpuCtx().ptr, nullptr);
    adapter.AbandonGl();
    adapter.End(1);
    adapter.ReleaseGl();
    EXPECT_EQ(tracy::GetGpuCtx().ptr, nullptr);
    EXPECT_EQ(g_DeletedQueries, 65536u);
}

TEST(DiagnosticProfilingTest, SharedRuntimeAcrossInstrumentedModules)
{
    ncl_perf::Profiling_Start();
    EXPECT_TRUE(ncl_perf::Profiling_IsStarted());
    nitro_utils::SysModule first = nitro_utils::LoadSysModule(PERF_PROBE_A_PATH);
    nitro_utils::SysModule second = nitro_utils::LoadSysModule(PERF_PROBE_B_PATH);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    using IdentityFn = uintptr_t (*)();
    IdentityFn first_identity = reinterpret_cast<IdentityFn>(nitro_utils::GetProcAddress(first, "PerfTest_RuntimeIdentity"));
    IdentityFn second_identity = reinterpret_cast<IdentityFn>(nitro_utils::GetProcAddress(second, "PerfTest_RuntimeIdentity"));
    ASSERT_NE(first_identity, nullptr);
    ASSERT_NE(second_identity, nullptr);
    EXPECT_NE(first_identity(), 0u);
    EXPECT_EQ(first_identity(), second_identity());
    EXPECT_EQ(first_identity(), ncl_perf::Profiling_RuntimeIdentity());
    using StatusFn = tracy::QueueMemoryStatus (*)();
    using AllocateFn = void* (*)(size_t);
    using FreeFn = void (*)(void*);
    StatusFn second_status = reinterpret_cast<StatusFn>(nitro_utils::GetProcAddress(second, "PerfTest_QueueMemoryStatus"));
    AllocateFn first_allocate = reinterpret_cast<AllocateFn>(nitro_utils::GetProcAddress(first, "PerfTest_QueueAllocate"));
    FreeFn second_free = reinterpret_cast<FreeFn>(nitro_utils::GetProcAddress(second, "PerfTest_QueueFree"));
    ASSERT_NE(second_status, nullptr);
    ASSERT_NE(first_allocate, nullptr);
    ASSERT_NE(second_free, nullptr);
    const size_t initial = second_status().used;
    void* allocation = first_allocate(1024u * 1024u);
    ASSERT_NE(allocation, nullptr);
    EXPECT_GT(second_status().used, initial);
    EXPECT_EQ(second_status().used, tracy::QueueMemory_GetStatus().used);
    EXPECT_EQ(second_status().limit, kTracyQueueMemoryLimit);
    second_free(allocation);
    EXPECT_EQ(second_status().used, initial);
    ncl_perf::Profiling_Stop();
    EXPECT_FALSE(ncl_perf::Profiling_IsStarted());
    nitro_utils::UnloadSysModule(second);
    nitro_utils::UnloadSysModule(first);
}

TEST(DiagnosticProfilingTest, ScopesAndMetadataAreSafeBeforeStartupAndAfterShutdown)
{
    EXPECT_FALSE(ncl_perf::Profiling_IsStarted());
    {
        NCL_PROFILE_SCOPE();
        NCL_PROFILE_VALUE("test value", 1);
        NCL_PROFILE_THREAD_NAME("diagnostic test");
    }
    EXPECT_EQ(ncl_perf::Profiling_RuntimeIdentity(), 0u);
}
TEST_F(DiagnosticGpuTest, MissingCalibrationPreservesCpuProfilingWithoutGpuContext)
{
    glad_glGetInteger64v = nullptr;
    performance::TracyGpuAdapter adapter;
    adapter.Initialize();
    EXPECT_EQ(tracy::GetGpuCtx().ptr, nullptr);
    EXPECT_TRUE(ncl_perf::Profiling_IsStarted());
    adapter.AbandonGl();
}

TEST_F(DiagnosticGpuTest, MissingExtensionDiscoveryPreservesCpuProfilingWithoutGpuContext)
{
    glad_glGetStringi = nullptr;
    performance::TracyGpuAdapter adapter;
    adapter.Initialize();
    EXPECT_EQ(tracy::GetGpuCtx().ptr, nullptr);
    EXPECT_TRUE(ncl_perf::Profiling_IsStarted());
    EXPECT_EQ(g_DeletedQueries, 0u);
}

TEST_F(DiagnosticGpuTest, QueryBufferObjectRetainsUnavailableTimestampsWithoutWaiting)
{
    g_QueryBufferObject = true;
    std::unique_ptr<tracy::GpuCtx> context = std::make_unique<tracy::GpuCtx>();
    unsigned int start = 0;
    unsigned int end = 0;
    ASSERT_TRUE(context->ReserveQueryPair(start, end));
    context->WriteTimestamp(start);
    g_Timestamp = 100;
    context->WriteTimestamp(end);
    context->Collect();
    EXPECT_EQ(g_NoWaitReads, 1u);
    EXPECT_EQ(g_ResultReads, 0u);
    EXPECT_EQ(g_AvailabilityReads, 0u);
    g_QueryReady = true;
    context->Collect();
    EXPECT_EQ(g_NoWaitReads, 3u);
    EXPECT_EQ(g_AvailabilityReads, 0u);
    EXPECT_EQ(g_ReadTimes, (std::vector<uint64_t>{0, 100}));
    context->ReleaseGl();
}

TEST_F(DiagnosticGpuTest, NestedResultsFollowTimestampSubmissionOrderAcrossLongFrames)
{
    std::unique_ptr<tracy::GpuCtx> context = std::make_unique<tracy::GpuCtx>();
    unsigned int parent_start = 0;
    unsigned int parent_end = 0;
    unsigned int child_start = 0;
    unsigned int child_end = 0;
    ASSERT_TRUE(context->ReserveQueryPair(parent_start, parent_end));
    context->WriteTimestamp(parent_start);
    ASSERT_TRUE(context->ReserveQueryPair(child_start, child_end));
    g_Timestamp = 1000000;
    context->WriteTimestamp(child_start);
    g_Timestamp = 2000000;
    context->WriteTimestamp(child_end);
    g_Timestamp = 3000000000;
    context->WriteTimestamp(parent_end);
    g_QueryReady = true;
    context->Collect();
    EXPECT_EQ(g_ReadTimes, (std::vector<uint64_t>{0, 1000000, 2000000, 3000000000}));
    context->ReleaseGl();
}
