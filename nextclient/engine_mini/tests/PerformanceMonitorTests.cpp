#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <next_engine_mini/PerformanceScopes.h>

#include "performance/GpuQueryBackend.h"
#include "performance/PerformanceMonitor.h"
#include "performance/PerformanceFrameSubscription.h"
#include "performance/performance_dump.h"
#include "performance/performance_stages.h"
#include "performance/PerformanceMetrics.h"
#include "performance/performance_integration.h"
#include "performance/PerformanceTraceBackend.h"

namespace
{
    class MonitorTrace : public performance::PerformanceTraceBackendInterface
    {
    public:
        uint32_t cpu_begins{};
        uint32_t cpu_ends{};
        uint32_t gpu_begins{};
        uint32_t gpu_ends{};
        uint32_t frames{};
        uint32_t outputs{};
        std::source_location cpu_location{};
        std::source_location gpu_location{};
        uint32_t gauge_samples{};
        std::array<double, uint32_t(performance::PerfGaugeId::Count)> gauges{};
        uint32_t counter_samples{};
        std::array<uint64_t, uint32_t(performance::PerfCounterId::Count)> counters{};
        bool connected = true;
        mutable uint32_t connection_reads{};
        uint32_t collections{};
        ncl_perf::PerfTimestamp_t* poll_time{};
        ncl_perf::PerfDuration_t poll_duration{};
        bool IsConnected() const override
        {
            ++connection_reads;
            return connected;
        }
        ncl_perf::PerfScopeToken_t BeginCpu(ncl_perf::PerfScopeId_t, std::source_location location) override
        {
            cpu_location = location;
            return ++cpu_begins;
        }
        void EndCpu(ncl_perf::PerfScopeToken_t) override
        {
            ++cpu_ends;
        }
        ncl_perf::PerfScopeToken_t BeginGpu(ncl_perf::PerfScopeId_t, std::source_location location) override
        {
            gpu_location = location;
            return ++gpu_begins;
        }
        void EndGpu(ncl_perf::PerfScopeToken_t) override
        {
            ++gpu_ends;
        }
        void Collect() override
        {
            ++collections;
            if (poll_time)
            {
                *poll_time += poll_duration;
            }
        }
        void RecordCounter(performance::PerfCounterId counter, uint64_t value) override
        {
            counters[uint32_t(counter)] = value;
            ++counter_samples;
        }
        void RecordGauge(performance::PerfGaugeId gauge, double value) override
        {
            gauges[uint32_t(gauge)] = value;
            ++gauge_samples;
        }
        void MarkFrame(bool output, uint64_t) override
        {
            if (output)
            {
                ++outputs;
            }
            else
            {
                ++frames;
            }
        }
    };
} // namespace

namespace
{
    class MonitorClock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t time{};
        uint32_t reads{};
        ncl_perf::PerfDuration_t read_step{};
        ncl_perf::PerfTimestamp_t Now() override
        {
            ++reads;
            time += read_step;
            return time;
        }
    };

    class MonitorGpu : public performance::GpuQueryBackendInterface
    {
    public:
        mutable uint32_t calls{};

        bool IsTimestampSupported() const override
        {
            ++calls;
            return true;
        }

        int counter_bits() const override
        {
            ++calls;
            return 64;
        }

        void Allocate(uint32_t count, uint32_t* names) override
        {
            ++calls;
            for (uint32_t index = 0; index < count; ++index)
            {
                names[index] = index + 1;
            }
        }

        void Delete(uint32_t, const uint32_t*) override
        {
            ++calls;
        }

        void Write(uint32_t) override
        {
            ++calls;
        }

        bool IsReady(uint32_t) override
        {
            ++calls;
            return false;
        }

        uint64_t Read(uint32_t) override
        {
            ++calls;
            return 0;
        }
    };

    class PerformanceMonitorTest : public testing::Test
    {
    protected:
        MonitorClock clock;
        performance::PerformanceMonitor monitor{clock};
        int window{};

        void Output(uint64_t end)
        {
            performance::PerfOutputId_t id = monitor.BeginOutput(&window);
            clock.time = end;
            monitor.EndOutput(id);
        }

        void Activate()
        {
            monitor.SetMainWindow(&window);
            monitor.SetMode(1);
        }
    };
} // namespace

TEST_F(PerformanceMonitorTest, ModeZeroDoesNoCollection)
{
    monitor.SetMainWindow(&window);
    for (int iteration = 0; iteration < 10; ++iteration)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::Radar), 0u);
        EXPECT_EQ(monitor.BeginGpu(performance::PerfStage::Radar), 0u);
        Output(100);
        monitor.OnHostExit();
    }
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(clock.reads, 0u);
    EXPECT_EQ(snapshot.record_count, 0u);
}

TEST_F(PerformanceMonitorTest, TraceScopesAndFrameMarkersWorkWithMonitorAndGpuDisabled)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.SetGpuEnabled(false);
    monitor.SetMainWindow(&window);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t cpu = monitor.BeginCpu(performance::PerfStage::Radar);
    ncl_perf::PerfScopeToken_t gpu = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    EXPECT_NE(cpu, 0u);
    EXPECT_NE(gpu, 0u);
    monitor.EndGpu(gpu);
    monitor.EndCpu(cpu);
    Output(100);
    monitor.OnHostExit();
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    EXPECT_EQ(trace.gpu_begins, trace.gpu_ends);
    EXPECT_EQ(trace.frames, 1u);
    EXPECT_EQ(trace.outputs, 1u);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, TraceCountersPublishPerAcceptedFrameWithoutBuiltInCollection)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.SetMainWindow(&window);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    for (uint32_t index = 0; index < uint32_t(performance::PerfCounterId::Count); ++index)
    {
        performance::PerfCounterId counter = performance::PerfCounterId(index);
        if (counter != performance::PerfCounterId::CpuDroppedScopes && counter != performance::PerfCounterId::GpuDroppedScopes &&
            counter != performance::PerfCounterId::GpuExpiredResults)
        {
            monitor.AddCounter(counter, index + 1);
            monitor.AddCounter(counter, 2);
        }
    }
    monitor.OnHostExit();

    ASSERT_EQ(trace.counter_samples, uint32_t(performance::PerfCounterId::Count));
    for (uint32_t index = 0; index < uint32_t(performance::PerfCounterId::Count); ++index)
    {
        performance::PerfCounterId counter = performance::PerfCounterId(index);
        bool diagnostic = counter == performance::PerfCounterId::CpuDroppedScopes ||
                          counter == performance::PerfCounterId::GpuDroppedScopes ||
                          counter == performance::PerfCounterId::GpuExpiredResults;
        EXPECT_EQ(trace.counters[index], diagnostic ? 0u : index + 3);
    }

    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    EXPECT_EQ(trace.counter_samples, 2 * uint32_t(performance::PerfCounterId::Count));
    for (uint64_t value : trace.counters)
    {
        EXPECT_EQ(value, 0u);
    }
    EXPECT_EQ(trace.frames, 2u);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, TraceCountersIgnoreRejectedAndDisconnectedFrames)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.OnHostEntry();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 50);
    monitor.OnHostExit();
    EXPECT_EQ(trace.counter_samples, 0u);

    trace.connected = false;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 100);
    monitor.OnHostExit();
    EXPECT_EQ(trace.counter_samples, 0u);

    trace.connected = true;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 3);
    monitor.OnHostExit();
    ASSERT_EQ(trace.counter_samples, uint32_t(performance::PerfCounterId::Count));
    EXPECT_EQ(trace.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 3u);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, TraceCountersReportActualDroppedScopes)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    std::array<ncl_perf::PerfScopeToken_t, ncl_perf::kPerfStackCapacity + 2> tokens{};
    for (ncl_perf::PerfScopeToken_t& token : tokens)
    {
        token = monitor.BeginCpu(performance::PerfStage::Radar);
    }
    for (auto token = tokens.rbegin(); token != tokens.rend(); ++token)
    {
        monitor.EndCpu(*token);
    }
    monitor.OnHostExit();

    ASSERT_EQ(trace.counter_samples, uint32_t(performance::PerfCounterId::Count));
    EXPECT_EQ(trace.counters[uint32_t(performance::PerfCounterId::CpuDroppedScopes)], 3u);
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, TraceCountersDiscardFrameDisconnectedBeforePublication)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 50);
    trace.connected = false;
    monitor.OnHostExit();
    EXPECT_EQ(trace.counter_samples, 0u);

    trace.connected = true;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    ASSERT_EQ(trace.counter_samples, uint32_t(performance::PerfCounterId::Count));
    EXPECT_EQ(trace.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 0u);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, EveryAvailableCpuStageReachesTraceWithoutBuiltInCollection)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    uint32_t expected = 0;
    for (ncl_perf::PerfScopeId_t id = 1; id < ncl_perf::kPerfScopeCapacity; ++id)
    {
        if (id == performance::PerfStage::Host || !performance::stages::HasCapability(id, ncl_perf::PerfScopeCapability::Cpu))
        {
            continue;
        }

        ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(id);
        ASSERT_NE(token, 0u) << performance::stages::ScopeName(id);
        monitor.EndCpu(token);
        ++expected;
    }
    EXPECT_EQ(trace.cpu_begins, expected);
    EXPECT_EQ(trace.cpu_ends, expected);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, ResetKeepsNativeZonesBalancedAndOldMonitorTokensInvalid)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(performance::PerfStage::Radar);
    ncl_perf::PerfScopeToken_t gpu = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.Reset();
    clock.time = 100;
    monitor.EndCpu(token);
    monitor.EndCpu(token);
    monitor.EndGpu(gpu);
    monitor.OnHostExit();
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    EXPECT_EQ(trace.gpu_begins, trace.gpu_ends);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.record_count, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, NativeHostWrapsRejectedInvocationWithoutAnAcceptedMarker)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.OnHostEntry();
    EXPECT_EQ(trace.cpu_begins, 1u);
    monitor.OnHostExit();
    EXPECT_EQ(trace.cpu_ends, 1u);
    EXPECT_EQ(trace.frames, 0u);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, ShutdownInsideHostPreservesTheNativeParentUntilExit)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.Shutdown();
    EXPECT_EQ(trace.cpu_ends, 0u);
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::Radar), 0u);
    monitor.OnHostExit();
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    EXPECT_FALSE(monitor.is_inside_host());
    uint32_t reads = clock.reads;
    performance::PerfSnapshot snapshot;
    EXPECT_FALSE(monitor.GetSnapshot(snapshot));
    EXPECT_EQ(clock.reads, reads);
    EXPECT_EQ(snapshot.record_count, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, PanelCostIsPublishedSeparatelyFromSwap)
{
    Activate();
    performance::PerfOutputId_t output = monitor.BeginOutput(&window);
    ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(performance::PerfStage::Monitor);
    clock.time = 300;
    monitor.EndCpu(token);
    clock.time = 500;
    monitor.MarkSwapStart();
    clock.time = 1500;
    monitor.EndOutput(output);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::MonitorCpu)].duration_ns, 300u);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::MonitorCpu)].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::SwapCpu)].duration_ns, 1000u);
}

TEST_F(PerformanceMonitorTest, DumpCopiesMetadataAndTruncatesCallerBufferSafely)
{
    monitor.SetGraphicsInfo("Vendor", "Renderer", "Version");
    monitor.SetEngineInfo(144, 1, true, true, 8684);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    uint32_t required = performance::dump::Format(snapshot, nullptr, 0);
    std::string dump(required, '\0');
    EXPECT_EQ(performance::dump::Format(snapshot, dump.data(), required), required);
    EXPECT_NE(dump.find("engine=8684"), std::string::npos);
    EXPECT_NE(dump.find("fps_max=144.00"), std::string::npos);
    EXPECT_NE(dump.find("renderer=Renderer"), std::string::npos);
#if USE_PROFILER
    EXPECT_NE(dump.find("Tracy queues: used="), std::string::npos);
    EXPECT_NE(dump.find("limit=134217728 bytes stopped=0 reason=none"), std::string::npos);
#else
    EXPECT_EQ(dump.find("Tracy queues:"), std::string::npos);
#endif
    std::array<char, 17> clipped;
    clipped.fill('x');
    EXPECT_EQ(performance::dump::Format(snapshot, clipped.data(), 16), required);
    EXPECT_EQ(clipped[15], '\0');
    EXPECT_EQ(clipped[16], 'x');
}

TEST_F(PerformanceMonitorTest, EnablingDuringFrameWaitsForBoundary)
{
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.SetMode(1);
    clock.time = 100;
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].count, 0u);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time = 300000100;
    monitor.OnHostExit();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].count, 1u);
}

TEST_F(PerformanceMonitorTest, SnapshotIsCopiedAndConsistent)
{
    Activate();
    Output(1000000);
    Output(10000000);
    performance::PerfSnapshot copied;
    monitor.GetSnapshot(copied);
    std::array<char, 8192> dump{};
    performance::dump::Format(copied, dump.data(), uint32_t(dump.size()));
    std::string original(dump.data());
    Output(300000000);
    performance::dump::Format(copied, dump.data(), uint32_t(dump.size()));
    EXPECT_EQ(original, dump.data());
    EXPECT_EQ(copied.latest.output_id, 2u);
}

TEST_F(PerformanceMonitorTest, DiagnosticReadsDoNotQueryClockOrBackends)
{
    MonitorTrace trace;
    MonitorGpu backend;
    monitor.SetGpuEnabled(false);
    monitor.SetGpuBackend(&backend);
    monitor.OnContextCreated(&window);
    monitor.SetTraceBackend(&trace);
    Activate();

    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time = 100;
    monitor.OnHostExit();

    uint32_t clock_reads = clock.reads;
    uint32_t backend_calls = backend.calls;
    uint32_t connection_reads = trace.connection_reads;
    uint32_t collections = trace.collections;
    performance::PerfSnapshot snapshot;
    for (uint32_t index = 0; index < 3; ++index)
    {
        clock.time = 250000000 * (index + 1);
        EXPECT_TRUE(monitor.GetSnapshot(snapshot));
        EXPECT_EQ(snapshot.record_count, 1u);
        EXPECT_EQ(snapshot.cpu_frame_id, 1u);
        EXPECT_EQ(snapshot.cpu_frame_count, 1u);
        EXPECT_TRUE(snapshot.trace_active);
        EXPECT_TRUE(snapshot.gpu_supported);
        EXPECT_EQ(snapshot.timer_bits, 64);
    }

    EXPECT_EQ(clock.reads, clock_reads);
    EXPECT_EQ(backend.calls, backend_calls);
    EXPECT_EQ(trace.connection_reads, connection_reads);
    EXPECT_EQ(trace.collections, collections);

    monitor.SetTraceBackend(nullptr);
    monitor.SetGpuBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, WorkerGenerationRejectsCanceledMap)
{
    Activate();
    uint32_t old_generation = monitor.map_generation();
    monitor.OnMapChanged();
    monitor.ReportWorker({old_generation, 400, performance::PerfWorkerStatus::Completed});
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.worker.status, performance::PerfWorkerStatus::Inactive);
    monitor.ReportWorker({monitor.map_generation(), 500, performance::PerfWorkerStatus::Completed});
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.worker.duration_ns, 500u);
}

TEST_F(PerformanceMonitorTest, MapTransitionRetainsHistoryButClearsFeatureState)
{
    Activate();
    Output(1000000);
    Output(300000000);
    monitor.ReportWorker({monitor.map_generation(), 500, performance::PerfWorkerStatus::Completed});
    monitor.OnMapChanged();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.record_count, 2u);
    EXPECT_EQ(snapshot.worker.status, performance::PerfWorkerStatus::Inactive);
    EXPECT_EQ(snapshot.summaries[0].count, 1u);
}

TEST_F(PerformanceMonitorTest, UnavailablePresenterStillAllowsDump)
{
    monitor.SetMode(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time = 100;
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    ASSERT_TRUE(monitor.GetSnapshot(snapshot));
    EXPECT_FALSE(snapshot.presenter_available);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].count, 1u);
    EXPECT_NE(std::string(snapshot.coverage_reason).find("SDL"), std::string::npos);
}

TEST_F(PerformanceMonitorTest, DisconnectedDiagnosticConsumerDoesNoCollection)
{
    MonitorTrace trace;
    trace.connected = false;
    monitor.SetTraceBackend(&trace);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.mode, 0);
    EXPECT_FALSE(snapshot.trace_active);
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::Radar), 0u);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, ExcludesOtherWindow)
{
    Activate();
    int other{};
    EXPECT_EQ(monitor.BeginOutput(&other), 0u);
    Output(100);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.record_count, 1u);
}

TEST_F(PerformanceMonitorTest, CapturedOpenGlWindowWorksWithoutCurrentWindowExport)
{
    monitor.SetMode(1);
    int other = 0;
    monitor.OnWindowCreated(&other, false);
    EXPECT_EQ(monitor.BeginOutput(&other), 0u);
    monitor.OnWindowCreated(&window, true);
    monitor.OnWindowCreated(&other, true);
    EXPECT_NE(monitor.BeginOutput(&window), 0u);
    EXPECT_EQ(monitor.BeginOutput(&other), 0u);
    monitor.OnWindowDestroyed(&other);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_TRUE(snapshot.presenter_available);
    monitor.OnWindowDestroyed(&window);
    monitor.GetSnapshot(snapshot);
    EXPECT_FALSE(snapshot.presenter_available);
    EXPECT_EQ(monitor.BeginOutput(&window), 0u);
}

TEST(PerformancePresentationTest, WindowSizeFallbackIsLimitedToPreHighDpiSdl)
{
    EXPECT_TRUE(performance::presentation::UsesLegacyWindowSize(1, 3, 0, false));
    EXPECT_TRUE(performance::presentation::UsesLegacyWindowSize(2, 0, 0, false));
    EXPECT_FALSE(performance::presentation::UsesLegacyWindowSize(2, 0, 0, true));
    EXPECT_FALSE(performance::presentation::UsesLegacyWindowSize(2, 0, 1, false));
    EXPECT_FALSE(performance::presentation::UsesLegacyWindowSize(2, 30, 9, false));
    EXPECT_FALSE(performance::presentation::UsesLegacyWindowSize(3, 0, 0, false));
}

TEST_F(PerformanceMonitorTest, TwoSwapsShareHostWithoutDuplicatingHostCpu)
{
    Activate();
    Output(100);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    Output(1000);
    Output(2000);
    clock.time = 2500;
    monitor.OnHostExit();
    clock.time = 250000100;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.summaries[0].count, 2u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].count, 1u);
    EXPECT_EQ(snapshot.latest.frame_id, 1u);
    EXPECT_EQ(snapshot.latest.output_id, 3u);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Unavailable);
}

TEST_F(PerformanceMonitorTest, HostCompositionAccumulatesSwapsAndExcludesStandaloneOutputs)
{
    Activate();
    Output(100);
    clock.time = 250000100;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time = 250000200;
    performance::PerfOutputId_t first = monitor.BeginOutput(&window);
    monitor.MarkSwapStart();
    clock.time = 250000500;
    monitor.EndOutput(first);
    clock.time = 250000600;
    performance::PerfOutputId_t second = monitor.BeginOutput(&window);
    monitor.MarkSwapStart();
    clock.time = 250000800;
    monitor.EndOutput(second);
    clock.time = 250001000;
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    performance::PerfGroupedGraphs groups;
    monitor.GetPanelSnapshot(snapshot, &groups);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::SwapCpu)].count, 1u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::SwapCpu)].mean_ns, 500u);
    EXPECT_EQ(groups.cpu.back().total.maximum_ns, 900u);
    EXPECT_TRUE(groups.cpu.back().parts_available);
    EXPECT_EQ(groups.cpu.back().parts[2], 500u);
    monitor.Reset();
    monitor.GetPanelSnapshot(snapshot, &groups);
    EXPECT_EQ(groups.cpu.back().total.count, 0u);
}

TEST_F(PerformanceMonitorTest, ScreenOnlyOutputHasNoHostCpu)
{
    Activate();
    Output(100);
    Output(200);
    clock.time = 250000100;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.latest.frame_id, 0u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].count, 0u);
    EXPECT_EQ(snapshot.summaries[0].count, 1u);
}

TEST_F(PerformanceMonitorTest, PanelFpsUsesHalfASecondWithoutChangingHistoryStatistics)
{
    Activate();
    Output(0);
    for (uint64_t end = 100000000; end <= 1000000000; end += 100000000)
    {
        Output(end);
    }
    for (uint64_t end = 1010000000; end <= 1500000000; end += 10000000)
    {
        Output(end);
    }

    clock.time = 1750000000;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetPanelSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(snapshot.summaries[0].fps, 100.0);
    EXPECT_EQ(snapshot.summaries[0].count, 60u);
    EXPECT_EQ(snapshot.summaries[0].mean_ns, 25000000u);
    monitor.GetSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(snapshot.summaries[0].fps, 40.0);
    clock.time = 2000000000;
    monitor.AdvanceSnapshot();
    monitor.GetPanelSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(snapshot.summaries[0].fps, 0.0);
    monitor.Reset();
    monitor.GetPanelSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(snapshot.summaries[0].fps, 0.0);
}

TEST_F(PerformanceMonitorTest, FirstSwapOnlyEstablishesBaseline)
{
    Activate();
    Output(100);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.summaries[0].count, 0u);
    monitor.Reset();
    Output(300000000);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.summaries[0].count, 0u);
    EXPECT_EQ(snapshot.latest.output_id, 2u);
}

TEST_F(PerformanceMonitorTest, BatchingCountsAggregateAndExpireWithTheHistoryWindow)
{
    Activate();
    for (uint64_t packets : {3u, 5u})
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        monitor.AddCounter(performance::PerfCounterId::StudioPackets, packets);
        clock.time += 1000000000;
        monitor.OnHostExit();
    }

    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 8u);

    Output(13000000000);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 0u);
}

TEST_F(PerformanceMonitorTest, CompletedHostCountersAreNotRepeatedByALaterStandaloneOutput)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 3);
    Output(100);
    clock.time = 200;
    monitor.OnHostExit();
    Output(300);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 3u);
}

TEST_F(PerformanceMonitorTest, BatchingDumpRetainsExactCountsAndFlushReasons)
{
    performance::PerfSnapshot snapshot;
    snapshot.coverage_ns = 2000000000;
    snapshot.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 8;
    snapshot.counters[uint32_t(performance::PerfCounterId::StudioSinglePackets)] = 2;
    snapshot.counters[uint32_t(performance::PerfCounterId::StudioFlushBrush)] = 3;
    snapshot.counters[uint32_t(performance::PerfCounterId::StudioImmediateMutableSkin)] = 1;
    std::string dump(performance::dump::Format(snapshot, nullptr, 0), '\0');
    performance::dump::Format(snapshot, dump.data(), uint32_t(dump.size()));
    EXPECT_NE(dump.find("Studio shared GL packets=8"), std::string::npos);
    EXPECT_NE(dump.find("Studio single GL packets=2"), std::string::npos);
    EXPECT_NE(dump.find("Studio flush brush=3"), std::string::npos);
    EXPECT_NE(dump.find("Studio immediate remap=1"), std::string::npos);
}
TEST_F(PerformanceMonitorTest, BasicBatchingDoesNotReadClocksForDetailedStudioStages)
{
    Activate();
    monitor.SetBatchLevel(1);
    clock.read_step = 10;
    ncl_perf::PerfTimestamp_t before = clock.time;
    for (ncl_perf::PerfScopeId_t id = performance::PerfStage::StudioGlSave; id <= performance::PerfStage::StudioTextureSetup; ++id)
    {
        ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(id);
        EXPECT_EQ(token, 0u);
        if (token)
        {
            monitor.EndCpu(token);
        }
    }
    EXPECT_EQ(clock.time, before);
}

TEST_F(PerformanceMonitorTest, DetailedStudioIsDormantWithBatchingAndDumpDisabled)
{
    Activate();
    monitor.SetBatchLevel(0);
    uint32_t reads = clock.reads;
    for (ncl_perf::PerfScopeId_t id = performance::PerfStage::StudioGlSave; id <= performance::PerfStage::StudioTextureSetup; ++id)
    {
        ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(id);
        EXPECT_EQ(token, 0u);
        monitor.EndCpu(token);
    }
    EXPECT_EQ(clock.reads, reads);
}
TEST_F(PerformanceMonitorTest, StudioGlCostsAreCpuOnlyChildrenAndRemainInDump)
{
    Activate();
    monitor.SetBatchLevel(2);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    for (ncl_perf::PerfScopeId_t id :
         {performance::PerfStage::StudioGlSave,
          performance::PerfStage::StudioGlRestore,
          performance::PerfStage::StudioProgramBind,
          performance::PerfStage::StudioBufferBind,
          performance::PerfStage::StudioBoneUpload})
    {
        EXPECT_TRUE(performance::stages::HasCapability(id, ncl_perf::PerfScopeCapability::Cpu));
        EXPECT_FALSE(performance::stages::HasCapability(id, ncl_perf::PerfScopeCapability::Gpu));
        ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(id);
        EXPECT_NE(token, 0u);
        clock.time += 1000;
        monitor.EndCpu(token);
    }
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.TakeDumpCapture(snapshot));
    uint32_t measured = 0;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        const ncl_perf::PerfCpuSample& sample = snapshot.cpu_tree[index];
        if (sample.scope_id >= performance::PerfStage::StudioGlSave && sample.scope_id <= performance::PerfStage::StudioBoneUpload)
        {
            EXPECT_EQ(sample.parent_id, performance::PerfStage::Studio);
            EXPECT_EQ(sample.inclusive_ns, 1000u);
            EXPECT_EQ(sample.self_ns, 1000u);
            EXPECT_EQ(sample.calls, 1u);
            ++measured;
        }
        if (sample.scope_id == performance::PerfStage::Studio)
        {
            EXPECT_EQ(sample.self_ns, 0u);
            EXPECT_EQ(sample.inclusive_ns, 5000u);
        }
    }
    EXPECT_EQ(measured, 5u);
    std::string dump(performance::dump::Format(snapshot, nullptr, 0), '\0');
    performance::dump::Format(snapshot, dump.data(), uint32_t(dump.size()));
    EXPECT_NE(dump.find("GL state save: Measured"), std::string::npos);
    EXPECT_NE(dump.find("GL state restore: Measured"), std::string::npos);
    EXPECT_NE(dump.find("Program bind: Measured"), std::string::npos);
    EXPECT_NE(dump.find("Buffer binds: Measured"), std::string::npos);
    EXPECT_NE(dump.find("Bone upload: Measured"), std::string::npos);
}

TEST_F(PerformanceMonitorTest, BatchLevelClampsAndDoesNotInterruptOpenScopes)
{
    Activate();
    monitor.SetBatchLevel(99);
    EXPECT_EQ(monitor.get_batch_level(), 2);
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(performance::PerfStage::StudioBoneUpload);
    ASSERT_NE(token, 0u);
    monitor.SetBatchLevel(1);
    clock.time += 100;
    monitor.EndCpu(token);
    monitor.OnHostExit();
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::StudioBoneUpload), 0u);
    monitor.SetBatchLevel(-1);
    EXPECT_EQ(monitor.get_batch_level(), 0);
}

TEST_F(PerformanceMonitorTest, StudioDetailAccountsForParentsAndExclusiveTime)
{
    Activate();
    monitor.SetBatchLevel(2);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    for (ncl_perf::PerfScopeId_t parent :
         {performance::PerfStage::StudioGlSave, performance::PerfStage::StudioGlRestore, performance::PerfStage::Studio})
    {
        ncl_perf::PerfScopeToken_t group = parent == performance::PerfStage::Studio ? 0 : monitor.BeginCpu(parent);
        ncl_perf::PerfScopeId_t first = parent == performance::PerfStage::StudioGlSave ? performance::PerfStage::StudioModelStateCapture
                             : parent == performance::PerfStage::StudioGlRestore ? performance::PerfStage::StudioRestoreBindings
                             : performance::PerfStage::StudioDataPrepare;
        ncl_perf::PerfScopeId_t last = parent == performance::PerfStage::StudioGlSave ? performance::PerfStage::StudioBatchStateSave
                            : parent == performance::PerfStage::StudioGlRestore ? performance::PerfStage::StudioRestoreTextures
                            : performance::PerfStage::StudioTextureSetup;
        for (ncl_perf::PerfScopeId_t id = first; id <= last; ++id)
        {
            EXPECT_TRUE(performance::stages::HasCapability(id, ncl_perf::PerfScopeCapability::Cpu));
            EXPECT_FALSE(performance::stages::HasCapability(id, ncl_perf::PerfScopeCapability::Gpu));
            ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(id);
            EXPECT_NE(token, 0u);
            clock.time += 100;
            monitor.EndCpu(token);
        }
        if (group)
        {
            monitor.EndCpu(group);
        }
    }
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.TakeDumpCapture(snapshot));
    uint32_t details = 0;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        const ncl_perf::PerfCpuSample& sample = snapshot.cpu_tree[index];
        if (sample.scope_id >= performance::PerfStage::StudioModelStateCapture &&
            sample.scope_id <= performance::PerfStage::StudioTextureSetup)
        {
            ncl_perf::PerfScopeId_t parent =
                sample.scope_id <= performance::PerfStage::StudioBatchStateSave    ? performance::PerfStage::StudioGlSave
                : sample.scope_id <= performance::PerfStage::StudioRestoreTextures ? performance::PerfStage::StudioGlRestore
                                                                                   : performance::PerfStage::Studio;
            EXPECT_EQ(sample.parent_id, parent);
            EXPECT_EQ(sample.inclusive_ns, 100u);
            EXPECT_EQ(sample.self_ns, 100u);
            ++details;
        }
        if (sample.scope_id == performance::PerfStage::Studio || sample.scope_id == performance::PerfStage::StudioGlSave ||
            sample.scope_id == performance::PerfStage::StudioGlRestore)
        {
            EXPECT_EQ(sample.self_ns, 0u);
        }
    }
    EXPECT_EQ(details, 10u);
    std::string dump(performance::dump::Format(snapshot, nullptr, 0), '\0');
    performance::dump::Format(snapshot, dump.data(), uint32_t(dump.size()));
    EXPECT_NE(dump.find("Model state capture: Measured"), std::string::npos);
    EXPECT_NE(dump.find("Batch state save: Measured"), std::string::npos);
    EXPECT_NE(dump.find("Restore bindings: Measured"), std::string::npos);
    EXPECT_NE(dump.find("Mesh sorting: Measured"), std::string::npos);
}

TEST_F(PerformanceMonitorTest, NumericCpuWindowRetainsStagesBetweenRefreshes)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time = 100;
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time = 200;
    monitor.OnHostExit();

    clock.time = 249000000;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);
    clock.time = 250000000;
    monitor.AdvanceSnapshot();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_count, 2u);
    bool found = false;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        if (snapshot.cpu_tree[index].scope_id == performance::PerfStage::Studio)
        {
            found = true;
            EXPECT_EQ(snapshot.cpu_tree[index].inclusive_ns, 100u);
            EXPECT_EQ(snapshot.cpu_tree[index].calls, 1u);
        }
    }
    EXPECT_TRUE(found);
    clock.time += 1;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 3u);
}

TEST_F(PerformanceMonitorTest, ReportsCpuCollectionAndBothGpuPollBoundaries)
{
    Activate();
    clock.read_step = 10;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t stage = monitor.BeginCpu(performance::PerfStage::Studio);
    monitor.EndCpu(stage);
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_GT(snapshot.cpu_collection_ns, 0u);

    clock.read_step = 0;
    MonitorTrace trace;
    trace.poll_time = &clock.time;
    trace.poll_duration = 40;
    monitor.SetTraceBackend(&trace);
    performance::PerfOutputId_t output = monitor.BeginOutput(&window);
    ncl_perf::PerfScopeToken_t panel = monitor.BeginCpu(performance::PerfStage::Monitor);
    monitor.PollGpu();
    monitor.EndCpu(panel);
    monitor.MarkSwapStart();
    monitor.EndOutput(output);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_poll_ns, 80u);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::MonitorCpu)].duration_ns, 80u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, DumpCapturesOnlyTheNextCompleteAcceptedFrameWhileDisabled)
{
    monitor.SetMainWindow(&window);
    monitor.SetBatchLevel(0);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ASSERT_TRUE(monitor.RequestDumpCapture());
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::StudioBoneUpload), 0u);
    monitor.OnHostExit();
    EXPECT_EQ(clock.reads, 0u);

    monitor.OnHostEntry();
    monitor.OnHostExit();
    EXPECT_EQ(clock.reads, 0u);
    performance::PerfSnapshot result;
    EXPECT_FALSE(monitor.TakeDumpCapture(result));

    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_EQ(monitor.get_mode(), 0);
    EXPECT_NE(monitor.get_scope_monitor(), nullptr);
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    ncl_perf::PerfScopeToken_t detail = monitor.BeginCpu(performance::PerfStage::StudioBoneUpload);
    ASSERT_NE(detail, 0u);
    clock.time = 1000;
    monitor.EndCpu(detail);
    monitor.EndCpu(studio);
    monitor.OnHostEntry();
    monitor.OnHostExit();
    EXPECT_FALSE(monitor.TakeDumpCapture(result));
    monitor.OnHostExit();

    ASSERT_TRUE(monitor.TakeDumpCapture(result));
    EXPECT_EQ(result.mode, 0);
    EXPECT_EQ(result.cpu_frame_count, 1u);
    EXPECT_EQ(result.cpu_window_ns, 1000u);
    bool found = false;
    for (uint32_t index = 0; index < result.cpu_count; ++index)
    {
        const ncl_perf::PerfCpuSample& sample = result.cpu_tree[index];
        if (sample.scope_id == performance::PerfStage::StudioBoneUpload)
        {
            EXPECT_EQ(sample.calls, 1u);
            EXPECT_EQ(sample.inclusive_ns, 1000u);
            found = true;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_FALSE(monitor.TakeDumpCapture(result));
    ASSERT_NE(monitor.get_dump_snapshot(), nullptr);
    EXPECT_EQ(monitor.get_dump_snapshot()->cpu_frame_count, 1u);
    EXPECT_EQ(monitor.get_scope_monitor(), nullptr);
    EXPECT_EQ(monitor.get_studio_detail_monitor(), nullptr);

    uint32_t reads = clock.reads;
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::StudioBoneUpload), 0u);
        Output(2000);
        monitor.OnHostExit();
    }
    EXPECT_EQ(clock.reads, reads);
}

TEST_F(PerformanceMonitorTest, DumpPreservesLiveMonitoringAndIgnoresPanelRefreshesWithinCapture)
{
    Activate();
    monitor.SetBatchLevel(0);
    performance::PerfSnapshot live;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time = 100;
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.RequestDumpCapture());
    EXPECT_FALSE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t detail = monitor.BeginCpu(performance::PerfStage::StudioDataPrepare);
    ASSERT_NE(detail, 0u);
    clock.time = 100000100;
    monitor.EndCpu(detail);
    monitor.GetSnapshot(live);
    clock.time = 300000100;
    monitor.GetSnapshot(live);
    clock.time = 400000100;
    monitor.OnHostExit();
    EXPECT_FALSE(monitor.RequestDumpCapture());
    performance::PerfSnapshot captured;
    ASSERT_TRUE(monitor.TakeDumpCapture(captured));
    EXPECT_EQ(captured.cpu_frame_count, 1u);
    EXPECT_EQ(captured.cpu_window_ns, 400000000u);
    EXPECT_EQ(monitor.get_mode(), 1);
    EXPECT_EQ(monitor.get_batch_level(), 0);
    uint32_t reads = clock.reads;
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::StudioDataPrepare), 0u);
    EXPECT_EQ(clock.reads, reads);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time += 1000;
    monitor.OnHostExit();
    EXPECT_EQ(monitor.get_dump_snapshot()->cpu_frame_id, captured.cpu_frame_id);
    EXPECT_TRUE(monitor.RequestDumpCapture());
}

TEST_F(PerformanceMonitorTest, DumpCancellationClearsPendingAndFrozenCaptures)
{
    for (bool active : {false, true})
    {
        ASSERT_TRUE(monitor.RequestDumpCapture());
        if (active)
        {
            monitor.OnHostEntry();
            monitor.OnFrameAccepted();
        }
        monitor.Reset();
        if (active)
        {
            monitor.OnHostExit();
        }
        performance::PerfSnapshot result;
        EXPECT_FALSE(monitor.TakeDumpCapture(result));
        EXPECT_EQ(monitor.get_dump_snapshot(), nullptr);
        EXPECT_EQ(monitor.get_scope_monitor(), nullptr);
    }
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnMapChanged();
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.Shutdown();
    EXPECT_FALSE(monitor.RequestDumpCapture());
}

TEST_F(PerformanceMonitorTest, ModeChangeDuringDumpKeepsTheRequestedSettingAndClosesScopes)
{
    Activate();
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t detail = monitor.BeginCpu(performance::PerfStage::StudioDataPrepare);
    monitor.SetMode(0);
    clock.time = 100;
    monitor.EndCpu(detail);
    monitor.OnHostExit();
    performance::PerfSnapshot result;
    ASSERT_TRUE(monitor.TakeDumpCapture(result));
    EXPECT_EQ(result.mode, 0);
    EXPECT_EQ(monitor.get_mode(), 0);
    EXPECT_EQ(monitor.get_scope_monitor(), nullptr);
}

TEST_F(PerformanceMonitorTest, BatchPanelCollectsCountersWithoutCpuTimings)
{
    monitor.SetMainWindow(&window);
    monitor.SetBatchLevel(1);
    for (int index = 0; index < 2; ++index)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        uint32_t reads = clock.reads;
        EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::Studio), 0u);
        EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::StudioDataPrepare), 0u);
        EXPECT_EQ(clock.reads, reads);
        monitor.AddCounter(performance::PerfCounterId::StudioPackets, 3);
        clock.time += 10000000;
        monitor.OnHostExit();
    }
    clock.time = 260000000;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.mode, 0);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 6u);
    EXPECT_EQ(snapshot.cpu_frame_count, 0u);
    EXPECT_GT(snapshot.record_count, 0u);
    monitor.SetBatchLevel(0);
    uint32_t reads = clock.reads;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(clock.reads, reads);
}
TEST_F(PerformanceMonitorTest, CpuAndGpuScopesPreserveConstructorCallSites)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    const std::source_location caller = std::source_location::current();
    {
        const uint32_t line = __LINE__ + 1;
        performance::PerformanceCpuScope cpu(&monitor, performance::PerfStage::StudioGeometry);
        EXPECT_STREQ(trace.cpu_location.file_name(), __FILE__);
        EXPECT_STREQ(trace.cpu_location.function_name(), caller.function_name());
        EXPECT_EQ(trace.cpu_location.line(), line);
    }
    {
        const uint32_t line = __LINE__ + 1;
        performance::PerformanceGpuScope gpu(&monitor, performance::PerfStage::StudioShader);
        EXPECT_STREQ(trace.gpu_location.file_name(), __FILE__);
        EXPECT_STREQ(trace.gpu_location.function_name(), caller.function_name());
        EXPECT_EQ(trace.gpu_location.line(), line);
    }
    EXPECT_EQ(clock.reads, 0u);
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    EXPECT_EQ(trace.gpu_begins, trace.gpu_ends);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, DetailedBatchingMeasuresContinuouslyWithOverviewDisabled)
{
    monitor.SetMode(0);
    monitor.SetBatchLevel(2);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_EQ(monitor.get_mode(), 0);
    EXPECT_NE(monitor.get_scope_monitor(), nullptr);
    EXPECT_NE(monitor.get_studio_detail_monitor(), nullptr);
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    ncl_perf::PerfScopeToken_t detail = monitor.BeginCpu(performance::PerfStage::StudioGlSave);
    EXPECT_NE(studio, 0u);
    EXPECT_NE(detail, 0u);
    clock.time += 100;
    monitor.EndCpu(detail);
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    EXPECT_FALSE(monitor.is_dump_ready());
    EXPECT_EQ(monitor.get_dump_snapshot(), nullptr);
}

TEST_F(PerformanceMonitorTest, LiveBatchingKeepsEightAcceptedFramesWithoutChangingOverviewHistory)
{
    Activate();
    monitor.SetBatchLevel(2);
    for (uint32_t frame = 1; frame <= 9; ++frame)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        monitor.AddCounter(performance::PerfCounterId::StudioPackets, frame);
        ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(performance::PerfStage::StudioGlSave);
        clock.time += frame * 100;
        monitor.EndCpu(token);
        monitor.OnHostExit();
        const performance::PerfSnapshot* live = monitor.get_batch_snapshot();
        ASSERT_NE(live, nullptr);
        EXPECT_EQ(live->cpu_frame_id, frame);
        EXPECT_EQ(live->cpu_frame_count, std::min(frame, 8u));
    }
    const performance::PerfSnapshot* live = monitor.get_batch_snapshot();
    EXPECT_EQ(live->counters[uint32_t(performance::PerfCounterId::StudioPackets)], 44u);
    bool found = false;
    for (uint32_t index = 0; index < live->cpu_count; ++index)
    {
        const ncl_perf::PerfCpuSample& sample = live->cpu_tree[index];
        if (sample.scope_id == performance::PerfStage::StudioGlSave)
        {
            found = true;
            EXPECT_EQ(sample.calls, 8u);
            EXPECT_EQ(sample.inclusive_ns, 4400u);
        }
    }
    EXPECT_TRUE(found);
    clock.time = 250000100;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot overview;
    monitor.GetSnapshot(overview);
    EXPECT_EQ(overview.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 45u);
    monitor.OnHostEntry();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 999);
    monitor.OnHostExit();
    Output(clock.time + 1);
    EXPECT_EQ(live->cpu_frame_id, 9u);
    EXPECT_EQ(live->counters[uint32_t(performance::PerfCounterId::StudioPackets)], 44u);
}

TEST_F(PerformanceMonitorTest, BasicBatchingIncludesEmptyFramesAndReplacesOldCounts)
{
    monitor.SetMode(0);
    monitor.SetBatchLevel(1);
    for (uint32_t frame = 0; frame < 9; ++frame)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        if (frame == 0)
        {
            monitor.AddCounter(performance::PerfCounterId::StudioPackets, 80);
        }
        clock.time += 100;
        monitor.OnHostExit();
    }
    const performance::PerfSnapshot* live = monitor.get_batch_snapshot();
    ASSERT_NE(live, nullptr);
    EXPECT_EQ(live->cpu_frame_count, 8u);
    EXPECT_EQ(live->cpu_count, 0u);
    EXPECT_EQ(live->counters[uint32_t(performance::PerfCounterId::StudioPackets)], 0u);
    EXPECT_EQ(monitor.get_scope_monitor(), nullptr);
    uint32_t reads = clock.reads;
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::StudioGlSave), 0u);
    EXPECT_EQ(clock.reads, reads);
}

TEST_F(PerformanceMonitorTest, BatchWindowClearsAcrossLevelsMapAndResetWithoutBreakingOpenScopes)
{
    monitor.SetMode(0);
    monitor.SetBatchLevel(2);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(performance::PerfStage::StudioGlSave);
    ASSERT_NE(token, 0u);
    monitor.SetBatchLevel(1);
    clock.time += 100;
    monitor.EndCpu(token);
    monitor.OnHostExit();
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 0u);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_EQ(monitor.get_scope_monitor(), nullptr);
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 3);
    monitor.OnHostExit();
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 1u);
    monitor.OnMapChanged();
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 0u);
    EXPECT_EQ(monitor.get_batch_snapshot()->counters[uint32_t(performance::PerfCounterId::StudioPackets)], 0u);
    monitor.SetBatchLevel(2);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 1u);
    monitor.Reset();
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 0u);
    monitor.SetBatchLevel(0);
    EXPECT_EQ(monitor.get_batch_snapshot(), nullptr);
    EXPECT_EQ(monitor.get_scope_monitor(), nullptr);
}

TEST_F(PerformanceMonitorTest, LiveBatchingAdvancesIndependentlyOfFrozenDump)
{
    monitor.SetMode(0);
    monitor.SetBatchLevel(2);
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time += 100;
    monitor.OnHostExit();
    performance::PerfSnapshot dump;
    ASSERT_TRUE(monitor.TakeDumpCapture(dump));
    performance::PerfFrameId_t dump_frame = dump.cpu_frame_id;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time += 100;
    monitor.OnHostExit();
    EXPECT_GT(monitor.get_batch_snapshot()->cpu_frame_id, dump_frame);
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 2u);
    EXPECT_EQ(monitor.get_dump_snapshot()->cpu_frame_id, dump_frame);
    EXPECT_EQ(monitor.get_dump_snapshot()->cpu_frame_count, 1u);
    EXPECT_EQ(monitor.get_mode(), 0);
}

TEST_F(PerformanceMonitorTest, ReturningToDetailedBatchingWithinAFrameRejectsPartialMeasurement)
{
    monitor.SetBatchLevel(2);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.SetBatchLevel(1);
    monitor.SetBatchLevel(2);
    monitor.OnHostExit();
    ASSERT_NE(monitor.get_batch_snapshot(), nullptr);
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 0u);
}

TEST_F(PerformanceMonitorTest, DisablingOverviewWithinAFramePreservesBasicBatchingCounters)
{
    Activate();
    monitor.SetBatchLevel(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 7);
    monitor.SetMode(0);
    monitor.OnHostExit();
    ASSERT_NE(monitor.get_batch_snapshot(), nullptr);
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 1u);
    EXPECT_EQ(monitor.get_batch_snapshot()->counters[uint32_t(performance::PerfCounterId::StudioPackets)], 7u);
}


TEST_F(PerformanceMonitorTest, CanceledScreensDoNotPublishTraceFrames)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    Activate();
    for (uint32_t index = 0; index < 3; ++index)
    {
        ncl_perf::PerfScopeToken_t screen = monitor.BeginGpu(performance::PerfStage::Screen);
        ASSERT_NE(screen, 0u);
        monitor.EndGpu(screen);
        monitor.EndGpu(screen);
    }
    EXPECT_EQ(trace.outputs, 0u);
    EXPECT_EQ(trace.gpu_begins, 3u);
    EXPECT_EQ(trace.gpu_ends, 3u);
    Output(100);
    EXPECT_EQ(trace.outputs, 1u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, AbortedHostRetriesDumpOnNextCompleteFrame)
{
    Activate();
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t abandoned = monitor.BeginCpu(performance::PerfStage::Packets);
    clock.time = 100;
    monitor.OnHostExit(false);
    EXPECT_FALSE(monitor.is_inside_host());
    EXPECT_FALSE(monitor.is_dump_ready());
    performance::PerfSnapshot partial;
    monitor.GetSnapshot(partial);
    EXPECT_EQ(partial.record_count, 0u);

    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t fresh = monitor.BeginCpu(performance::PerfStage::Radar);
    monitor.EndCpu(abandoned);
    clock.time = 140;
    monitor.EndCpu(fresh);
    clock.time = 160;
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.is_dump_ready());
    performance::PerfSnapshot captured;
    ASSERT_TRUE(monitor.TakeDumpCapture(captured));
    EXPECT_EQ(captured.cpu_frame_id, 2u);
    EXPECT_EQ(captured.cpu_frame_count, 1u);
    auto radar =
        std::find_if(captured.cpu_tree.begin(), captured.cpu_tree.begin() + captured.cpu_count, [](const ncl_perf::PerfCpuSample& sample) {
            return sample.scope_id == performance::PerfStage::Radar;
        });
    ASSERT_NE(radar, captured.cpu_tree.begin() + captured.cpu_count);
    EXPECT_EQ(radar->inclusive_ns, 40u);
    EXPECT_EQ(radar->calls, 1u);
}

TEST_F(PerformanceMonitorTest, AbortedHostClosesNativeScopesWithCollectionDisabled)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t cpu = monitor.BeginCpu(performance::PerfStage::Packets);
    ncl_perf::PerfScopeToken_t gpu = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.OnHostExit(false);
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    EXPECT_EQ(trace.gpu_begins, trace.gpu_ends);
    EXPECT_EQ(trace.frames, 0u);
    monitor.EndCpu(cpu);
    monitor.EndGpu(gpu);
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    EXPECT_EQ(trace.gpu_begins, trace.gpu_ends);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    EXPECT_EQ(trace.frames, 1u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, NestedAbortDiscardsOuterFrameAndAcceptsTheNextOne)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t outer = monitor.BeginCpu(performance::PerfStage::Packets);
    monitor.OnHostEntry();
    ASSERT_NE(monitor.BeginCpu(performance::PerfStage::Radar), 0u);
    monitor.OnHostExit(false);
    monitor.EndCpu(outer);
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    performance::PerfSnapshot partial;
    monitor.GetSnapshot(partial);
    EXPECT_EQ(partial.record_count, 0u);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    clock.time = 300000000;
    monitor.OnHostExit();
    performance::PerfSnapshot complete;
    monitor.GetSnapshot(complete);
    EXPECT_EQ(complete.record_count, 1u);
    EXPECT_FALSE(monitor.is_inside_host());
}

TEST_F(PerformanceMonitorTest, AbortedCaptureCanRestartWithPerformanceDisabled)
{
    monitor.SetMainWindow(&window);
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ASSERT_NE(monitor.BeginCpu(performance::PerfStage::Packets), 0u);
    monitor.OnHostExit(false);
    EXPECT_FALSE(monitor.is_dump_ready());
    monitor.OnHostEntry();
    EXPECT_EQ(monitor.get_mode(), 0);
    monitor.OnFrameAccepted();
    ASSERT_NE(monitor.get_scope_monitor(), nullptr);
    ncl_perf::PerfScopeToken_t radar = monitor.BeginCpu(performance::PerfStage::Radar);
    clock.time += 100;
    monitor.EndCpu(radar);
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.is_dump_ready());
    performance::PerfSnapshot captured;
    EXPECT_TRUE(monitor.TakeDumpCapture(captured));
    monitor.OnHostEntry();
    monitor.OnHostExit();
    EXPECT_EQ(monitor.get_mode(), 0);
}

TEST_F(PerformanceMonitorTest, PanelFrameWindowUsesOriginalOutputsAndResizesWithinTheSummaryCache)
{
    Activate();
    Output(100);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    Output(500);
    Output(800);
    clock.time = 1000;
    monitor.OnHostExit();
    clock.time = 250000100;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    performance::PerfFrameGraphs frames;
    monitor.GetPanelSnapshot(snapshot, nullptr, &frames, 4);
    ASSERT_EQ(frames.frames.size(), 4u);
    ASSERT_EQ(frames.count, 3u);
    EXPECT_EQ(frames.frames[2].output_id, 2u);
    EXPECT_EQ(frames.frames[2].values[uint32_t(performance::PerfSeries::HostCpu)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(frames.frames[3].output_id, 3u);
    EXPECT_EQ(frames.frames[3].values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns, 900u);
    ncl_perf::PerfTimestamp_t captured = frames.captured_at;
    monitor.GetPanelSnapshot(snapshot, nullptr, &frames, 7);
    ASSERT_EQ(frames.frames.size(), 7u);
    EXPECT_EQ(frames.captured_at, captured);
    EXPECT_EQ(frames.frames[6].output_id, 3u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].count, 1u);
    monitor.Reset();
    monitor.GetPanelSnapshot(snapshot, nullptr, &frames, 7);
    EXPECT_EQ(frames.count, 0u);
    ASSERT_EQ(frames.frames.size(), 7u);
    EXPECT_EQ(frames.frames.back().output_id, 0u);
}

TEST_F(PerformanceMonitorTest, PanelSummariesUseTheSameFrameWindowWithoutChangingDiagnosticHistory)
{
    Activate();
    Output(100);
    Output(1000);
    Output(1010);
    Output(1030);
    Output(1060);
    clock.time = 250000100;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot panel;
    performance::PerfSnapshot diagnostic;
    performance::PerfFrameGraphs frames;
    monitor.GetPanelSnapshot(panel, nullptr, &frames, 2);
    const performance::PerfSummary& visible = panel.summaries[uint32_t(performance::PerfSeries::FrameInterval)];
    EXPECT_EQ(visible.count, 2u);
    EXPECT_EQ(visible.mean_ns, 25u);
    EXPECT_EQ(visible.maximum_ns, 30u);
    EXPECT_FALSE(visible.percentiles_available);
    ASSERT_EQ(frames.frames.size(), 2u);
    EXPECT_EQ(frames.frames[0].output_id, 4u);
    EXPECT_EQ(frames.frames[1].output_id, 5u);

    monitor.GetSnapshot(diagnostic);
    const performance::PerfSummary& retained = diagnostic.summaries[uint32_t(performance::PerfSeries::FrameInterval)];
    EXPECT_EQ(retained.count, 4u);
    EXPECT_EQ(retained.mean_ns, 240u);
    EXPECT_EQ(retained.maximum_ns, 900u);
    monitor.GetPanelSnapshot(panel, nullptr, nullptr, 3);
    EXPECT_EQ(visible.count, 3u);
    EXPECT_EQ(visible.mean_ns, 20u);
    EXPECT_EQ(visible.maximum_ns, 30u);
}

TEST_F(PerformanceMonitorTest, CatalogCpuSourcesPopulateOnlyTheirHostSeries)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();

    for (const performance::SeriesDescriptor& series : performance::kSeries)
    {
        if (series.source != performance::SeriesSource::CpuInclusive || series.scope_id == performance::PerfStage::Host)
        {
            continue;
        }

        ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(series.scope_id);
        ASSERT_NE(token, 0u) << series.name;
        clock.time += 100 + uint32_t(series.id);
        monitor.EndCpu(token);
    }

    clock.time += 1000;
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);

    for (const performance::SeriesDescriptor& series : performance::kSeries)
    {
        const performance::PerfSummary& summary = snapshot.summaries[uint32_t(series.id)];
        SCOPED_TRACE(series.name);

        if (series.source == performance::SeriesSource::CpuInclusive)
        {
            EXPECT_EQ(summary.count, 1u);
            EXPECT_EQ(summary.state, ncl_perf::PerfSampleState::Measured);

            if (series.scope_id != performance::PerfStage::Host)
            {
                EXPECT_EQ(summary.mean_ns, 100 + uint32_t(series.id));
            }
        }
        else
        {
            EXPECT_EQ(summary.count, 0u);
        }
    }
}

TEST_F(PerformanceMonitorTest, CatalogCollectionLevelGatesEveryDetailedCpuStage)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();

    for (const performance::StageDescriptor& stage : performance::kStages)
    {
        if (stage.cpu_level == performance::CollectionLevel::StudioDetail)
        {
            EXPECT_EQ(monitor.BeginCpu(stage.scope.scope_id), 0u) << stage.scope.name;
        }
    }

    monitor.OnHostExit();
    monitor.SetBatchLevel(2);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();

    for (const performance::StageDescriptor& stage : performance::kStages)
    {
        if (stage.cpu_level == performance::CollectionLevel::StudioDetail)
        {
            ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(stage.scope.scope_id);
            EXPECT_NE(token, 0u) << stage.scope.name;
            monitor.EndCpu(token);
        }
    }

    monitor.OnHostExit();
}

TEST_F(PerformanceMonitorTest, InvalidCounterIdCannotWriteCounterStorage)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.AddCounter(performance::PerfCounterId(UINT32_MAX), 100);
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 3);
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 3u);
}

TEST_F(PerformanceMonitorTest, ProducerBoundaryPublishesWithoutDiagnosticReaders)
{
    Activate();
    for (uint32_t index = 0; index < 2; ++index)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        clock.time = index * 300000000;
        monitor.OnHostExit();
    }

    uint32_t reads = clock.reads;
    performance::PerfSnapshot snapshot;
    ASSERT_TRUE(monitor.GetSnapshot(snapshot));
    EXPECT_EQ(clock.reads, reads);
    EXPECT_EQ(snapshot.captured_at, 300000000u);
    EXPECT_EQ(snapshot.record_count, 2u);
    EXPECT_EQ(snapshot.cpu_frame_id, 2u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
}

TEST_F(PerformanceMonitorTest, PanelAndDiagnosticReadOrderPreservesPublishedWindows)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    Output(100);
    Output(200);
    Output(300);
    clock.time = 400;
    monitor.OnHostExit();
    performance::PerfSnapshot diagnostic;
    performance::PerfSnapshot panel;
    performance::PerfFrameGraphs frames;
    monitor.GetPanelSnapshot(panel, nullptr, &frames, 1);
    monitor.GetSnapshot(diagnostic);
    EXPECT_EQ(diagnostic.summaries[uint32_t(performance::PerfSeries::FrameInterval)].count, 2u);
    EXPECT_EQ(panel.summaries[uint32_t(performance::PerfSeries::FrameInterval)].count, 1u);
    EXPECT_EQ(panel.cpu_frame_count, diagnostic.cpu_frame_count);
    EXPECT_EQ(panel.cpu_window_ns, diagnostic.cpu_window_ns);
    ncl_perf::PerfTimestamp_t captured_at = diagnostic.captured_at;
    ncl_perf::PerfDuration_t cpu_window_ns = diagnostic.cpu_window_ns;
    uint32_t reads = clock.reads;

    clock.time = 900000000;
    monitor.GetSnapshot(diagnostic);
    monitor.GetPanelSnapshot(panel, nullptr, &frames, 2);
    EXPECT_EQ(diagnostic.captured_at, captured_at);
    EXPECT_EQ(panel.captured_at, captured_at);
    EXPECT_EQ(panel.cpu_window_ns, cpu_window_ns);
    EXPECT_EQ(panel.cpu_frame_count, 1u);
    EXPECT_EQ(diagnostic.summaries[uint32_t(performance::PerfSeries::FrameInterval)].count, 2u);
    EXPECT_EQ(panel.summaries[uint32_t(performance::PerfSeries::FrameInterval)].count, 2u);
    EXPECT_EQ(clock.reads, reads);
}

TEST_F(PerformanceMonitorTest, GpuInvalidationPreservesThePendingNumericCpuWindow)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    clock.time = 100000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time += 20;
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    monitor.SetGpuEnabled(false);
    clock.time = 249000000;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);

    clock.time = 250000000;
    monitor.AdvanceSnapshot();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 2u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    EXPECT_EQ(snapshot.cpu_window_ns, 250000000u);
    bool found = false;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        if (snapshot.cpu_tree[index].scope_id == performance::PerfStage::Studio)
        {
            found = true;
            EXPECT_EQ(snapshot.cpu_tree[index].inclusive_ns, 20u);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(PerformanceMonitorTest, DumpCapturePreservesThePartiallyAccumulatedLiveCpuWindow)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    clock.time = 100000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time += 20;
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.RequestDumpCapture());
    clock.time = 200000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    studio = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time += 30;
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    performance::PerfSnapshot captured;
    ASSERT_TRUE(monitor.TakeDumpCapture(captured));
    EXPECT_EQ(captured.cpu_frame_id, 3u);
    EXPECT_EQ(captured.cpu_frame_count, 1u);
    EXPECT_EQ(captured.captured_at, 200000030u);
    EXPECT_EQ(captured.record_count, 3u);

    clock.time = 250000000;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 3u);
    EXPECT_EQ(snapshot.cpu_frame_count, 2u);
    EXPECT_EQ(snapshot.cpu_window_ns, 250000000u);
    bool found = false;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        if (snapshot.cpu_tree[index].scope_id == performance::PerfStage::Studio)
        {
            found = true;
            EXPECT_EQ(snapshot.cpu_tree[index].inclusive_ns, 50u);
            EXPECT_EQ(snapshot.cpu_tree[index].calls, 2u);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(PerformanceMonitorTest, AbortedFrameDoesNotEnterPendingNumericCpuWindow)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    clock.time = 100000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t stale = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time += 20;
    monitor.OnHostExit(false);
    monitor.EndCpu(stale);
    clock.time = 200000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time += 30;
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    clock.time = 250000000;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.record_count, 2u);
    EXPECT_EQ(snapshot.cpu_frame_id, 3u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        if (snapshot.cpu_tree[index].scope_id == performance::PerfStage::Studio)
        {
            EXPECT_EQ(snapshot.cpu_tree[index].inclusive_ns, 30u);
            EXPECT_EQ(snapshot.cpu_tree[index].calls, 1u);
        }
    }
}

TEST_F(PerformanceMonitorTest, TraceMetadataRefreshBelongsToTheExplicitProducerBoundary)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    Activate();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_TRUE(snapshot.trace_active);
    trace.connected = false;
    monitor.GetSnapshot(snapshot);
    EXPECT_TRUE(snapshot.trace_active);
    monitor.AdvanceSnapshot();
    monitor.GetSnapshot(snapshot);
    EXPECT_FALSE(snapshot.trace_active);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, MapChangeClearsPendingCpuBeforeTheNextAcceptedFrame)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    clock.time = 100000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time += 20;
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    monitor.OnMapChanged();
    clock.time = 200000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    studio = monitor.BeginCpu(performance::PerfStage::Studio);
    clock.time += 30;
    monitor.EndCpu(studio);
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 3u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    bool found = false;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        if (snapshot.cpu_tree[index].scope_id == performance::PerfStage::Studio)
        {
            found = true;
            EXPECT_EQ(snapshot.cpu_tree[index].inclusive_ns, 30u);
            EXPECT_EQ(snapshot.cpu_tree[index].calls, 1u);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(PerformanceMonitorTest, GaugeReplacesAbsoluteObservationAndKeepsLatestOutputIdentity)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 3));
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 7));
    Output(100);
    clock.time = 200;
    monitor.OnHostExit();

    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    const performance::PerfScalarValue& value = snapshot.gauges[uint32_t(performance::PerfGaugeId::StudioQueuePeakPackets)];
    EXPECT_EQ(value.state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(value.value, 7);
    EXPECT_EQ(snapshot.latest.kind, performance::PerfRecordKind::Output);
    EXPECT_EQ(snapshot.latest.output_id, 1u);
    EXPECT_EQ(snapshot.latest.completed_at, 100u);
}

TEST_F(PerformanceMonitorTest, GaugeZeroIsMeasuredAndMissingHostDoesNotCarryForward)
{
    Activate();
    for (uint32_t frame = 0; frame < 3; ++frame)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        if (frame < 2)
        {
            EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, frame == 0 ? 8 : 0));
        }
        clock.time += 300000000;
        monitor.OnHostExit();
    }

    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    const performance::PerfScalarValue& value = snapshot.gauges[uint32_t(performance::PerfGaugeId::StudioQueuePeakPackets)];
    const performance::PerfScalarSummary& summary = snapshot.gauge_summaries[uint32_t(performance::PerfGaugeId::StudioQueuePeakPackets)];
    EXPECT_EQ(value.state, ncl_perf::PerfSampleState::Inactive);
    EXPECT_EQ(summary.state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(summary.count, 2u);
    EXPECT_DOUBLE_EQ(summary.last, 0);
    EXPECT_DOUBLE_EQ(summary.mean, 4);
    EXPECT_DOUBLE_EQ(summary.maximum, 8);
}

TEST_F(PerformanceMonitorTest, GaugeRejectsInvalidWritesWithoutReplacingPendingValue)
{
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 1));
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 2));
    monitor.OnHostExit();
    Activate();
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 3));
    monitor.OnHostEntry();
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 4));
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, -3));
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::Count, 8));
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId(uint32_t(-1)), 9));
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, std::numeric_limits<double>::quiet_NaN()));
    clock.time = 300000000;
    monitor.OnHostExit();

    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(snapshot.gauges[0].value, -3);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].maximum, -3);
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 10));
}

TEST_F(PerformanceMonitorTest, GaugeLatestHostRefreshDoesNotWaitForHistoryOrCpuWindow)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 3));
    monitor.OnHostExit();
    clock.time = 100000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 7));
    monitor.OnHostExit();

    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_DOUBLE_EQ(snapshot.gauges[0].value, 7);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gauges[0].state, ncl_perf::PerfSampleState::Inactive);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);
    EXPECT_EQ(snapshot.gauge_summaries[0].count, 1u);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].last, 3);
}

TEST_F(PerformanceMonitorTest, GaugeResetMapAbortAndModeChangeDiscardPendingObservations)
{
    for (int transition = 0; transition < 4; ++transition)
    {
        Activate();
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 97));
        switch (transition)
        {
            case 0:
                monitor.Reset();
                break;
            case 1:
                monitor.OnMapChanged();
                break;
            case 2:
                monitor.OnHostExit(false);
                break;
            default:
                monitor.SetMode(0);
                break;
        }
        if (transition != 2)
        {
            monitor.OnHostExit();
        }
        Activate();
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        clock.time += 300000000;
        monitor.OnHostExit();
        performance::PerfSnapshot snapshot;
        monitor.GetSnapshot(snapshot);
        EXPECT_EQ(snapshot.gauges[0].state, ncl_perf::PerfSampleState::Inactive) << transition;
        EXPECT_EQ(snapshot.gauge_summaries[0].count, 0u) << transition;
        monitor.Reset();
    }
}

TEST_F(PerformanceMonitorTest, GaugeTraceOnlyPublishesMeasuredLastObservationAtAcceptedCommit)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 3));
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 7));
    EXPECT_EQ(trace.gauge_samples, 0u);
    monitor.OnHostExit();
    EXPECT_EQ(trace.gauge_samples, 1u);
    EXPECT_DOUBLE_EQ(trace.gauges[0], 7);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    EXPECT_EQ(trace.gauge_samples, 1u);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 0));
    monitor.OnHostExit();
    EXPECT_EQ(trace.gauge_samples, 2u);
    EXPECT_DOUBLE_EQ(trace.gauges[0], 0);
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, GaugeTraceOmitsRejectedAbortedAndDisconnectedFrames)
{
    MonitorTrace trace;
    monitor.SetTraceBackend(&trace);
    monitor.OnHostEntry();
    EXPECT_FALSE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 4));
    monitor.OnHostExit();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 5));
    monitor.OnHostExit(false);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 6));
    trace.connected = false;
    monitor.OnHostExit();
    EXPECT_EQ(trace.gauge_samples, 0u);
    trace.connected = true;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    EXPECT_EQ(trace.gauge_samples, 0u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, GaugeDumpFreezesCapturedHostInsideHistoryRefreshInterval)
{
    Activate();
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 3));
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.RequestDumpCapture());
    clock.time = 100000000;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 7));
    monitor.OnHostExit();
    performance::PerfSnapshot captured;
    ASSERT_TRUE(monitor.TakeDumpCapture(captured));
    EXPECT_EQ(captured.gauges[0].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(captured.gauges[0].value, 7);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 11));
    monitor.OnHostExit();
    EXPECT_DOUBLE_EQ(captured.gauges[0].value, 7);
    uint32_t required = performance::dump::Format(captured, nullptr, 0);
    std::string text(required, '\0');
    performance::dump::Format(captured, text.data(), required);
    EXPECT_NE(text.find("last=7 packets"), std::string::npos);
}

TEST_F(PerformanceMonitorTest, GaugeDumpFormatsInactiveCurrentWithoutInventingZero)
{
    performance::PerfSnapshot snapshot;
    snapshot.gauges[0] = {0, ncl_perf::PerfSampleState::Inactive};
    snapshot.gauge_summaries[0] = {2, 7, 5, 7, ncl_perf::PerfSampleState::Measured};
    uint32_t required = performance::dump::Format(snapshot, nullptr, 0);
    std::string text(required, '\0');
    performance::dump::Format(snapshot, text.data(), required);
    EXPECT_NE(text.find("last=Inactive"), std::string::npos);
    EXPECT_NE(text.find("max=7 packets"), std::string::npos);
    EXPECT_EQ(text.find("last=0 packets"), std::string::npos);
    snapshot.gauges[0] = {0, ncl_perf::PerfSampleState::Measured};
    required = performance::dump::Format(snapshot, nullptr, 0);
    text.resize(required);
    performance::dump::Format(snapshot, text.data(), required);
    EXPECT_NE(text.find("last=0 packets"), std::string::npos);
}

TEST_F(PerformanceMonitorTest, GaugeBatchOnlyCollectsWithoutCpuStages)
{
    monitor.SetBatchLevel(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 6));
    monitor.OnHostExit();
    const performance::PerfSnapshot* snapshot = monitor.get_batch_snapshot();
    ASSERT_NE(snapshot, nullptr);
    EXPECT_EQ(snapshot->gauges[0].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(snapshot->gauges[0].value, 6);
    EXPECT_EQ(snapshot->gauge_summaries[0].count, 1u);
    EXPECT_EQ(snapshot->cpu_count, 0u);
}

TEST_F(PerformanceMonitorTest, GaugeCaptureActivatesCollectionWhenLiveMonitorIsDisabled)
{
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, 4));
    monitor.OnHostExit();
    performance::PerfSnapshot captured;
    ASSERT_TRUE(monitor.TakeDumpCapture(captured));
    EXPECT_EQ(captured.gauges[0].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(captured.gauges[0].value, 4);
    EXPECT_EQ(monitor.get_mode(), 0);
}
TEST_F(PerformanceMonitorTest, GaugeHistoryReducesThreeAcceptedHosts)
{
    Activate();
    for (double value : {9.0, 3.0, 6.0})
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        EXPECT_TRUE(monitor.SetGauge(performance::PerfGaugeId::StudioQueuePeakPackets, value));
        clock.time += 300000000;
        monitor.OnHostExit();
    }
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gauge_summaries[0].count, 3u);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].last, 6);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].mean, 6);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].maximum, 9);
}

namespace
{
    class MonitorLifecycle : public nitroapi::FrameLifecycleInterface
    {
    private:
        nitroapi::FrameLifecycleSubscription_t next_subscription_{};

    public:
        std::map<nitroapi::FrameLifecycleSubscription_t, nitroapi::FrameLifecycleObserverInterface*> observers;
        std::vector<nitroapi::FrameLifecycleSubscription_t> removed;
        std::function<void()> on_subscribe;
        std::function<void()> on_unsubscribe;

        nitroapi::FrameLifecycleSubscription_t Subscribe(nitroapi::FrameLifecycleObserverInterface* observer) override
        {
            if (on_subscribe)
            {
                on_subscribe();
            }

            nitroapi::FrameLifecycleSubscription_t subscription = ++next_subscription_;
            observers.emplace(subscription, observer);
            return subscription;
        }

        void Unsubscribe(nitroapi::FrameLifecycleSubscription_t subscription) override
        {
            if (on_unsubscribe)
            {
                on_unsubscribe();
            }

            removed.push_back(subscription);
            observers.erase(subscription);
        }
    };
} // namespace

TEST_F(PerformanceMonitorTest, FrameSubscriptionShutdownWaitsForNestedHostAndClosesTrace)
{
    MonitorTrace trace;
    MonitorLifecycle lifecycle;
    performance::FrameSubscription subscription(monitor);
    monitor.SetTraceBackend(&trace);
    subscription.Bind(&lifecycle);
    ASSERT_EQ(lifecycle.observers.size(), 1u);
    nitroapi::FrameLifecycleObserverInterface* observer = lifecycle.observers.begin()->second;
    observer->OnHostEntry();
    observer->OnFrameAccepted();
    observer->OnHostEntry();
    ASSERT_EQ(trace.cpu_begins, 1u);

    lifecycle.on_unsubscribe = [&] {
        EXPECT_FALSE(monitor.is_inside_host());
        EXPECT_EQ(trace.cpu_ends, 1u);
    };
    subscription.BeginShutdown();
    monitor.Shutdown();
    EXPECT_TRUE(subscription.is_bound());
    EXPECT_TRUE(monitor.is_inside_host());
    EXPECT_TRUE(lifecycle.removed.empty());
    EXPECT_EQ(trace.cpu_ends, 0u);

    observer->OnHostExit(true);
    EXPECT_TRUE(subscription.is_bound());
    EXPECT_TRUE(monitor.is_inside_host());
    EXPECT_TRUE(lifecycle.removed.empty());
    EXPECT_EQ(trace.cpu_ends, 0u);

    observer->OnHostExit(true);
    EXPECT_FALSE(monitor.is_inside_host());
    EXPECT_FALSE(subscription.is_bound());
    EXPECT_TRUE(lifecycle.observers.empty());
    ASSERT_EQ(lifecycle.removed.size(), 1u);
    EXPECT_EQ(trace.cpu_ends, 1u);
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, FrameSubscriptionRebindReleasesRetainedIdBeforeRegistering)
{
    MonitorTrace trace;
    MonitorLifecycle lifecycle;
    performance::FrameSubscription subscription(monitor);
    monitor.SetTraceBackend(&trace);
    subscription.Bind(&lifecycle);
    ASSERT_EQ(lifecycle.observers.size(), 1u);
    nitroapi::FrameLifecycleSubscription_t original = lifecycle.observers.begin()->first;
    nitroapi::FrameLifecycleObserverInterface* observer = lifecycle.observers.begin()->second;
    observer->OnHostEntry();
    subscription.BeginShutdown();
    monitor.Shutdown();
    observer->OnHostExit(true);
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);

    subscription.Bind(&lifecycle);
    ASSERT_EQ(lifecycle.observers.size(), 1u);
    EXPECT_GT(lifecycle.observers.begin()->first, original);
    nitroapi::FrameLifecycleSubscription_t rebound = lifecycle.observers.begin()->first;
    lifecycle.on_subscribe = [&] { EXPECT_TRUE(lifecycle.observers.empty()); };
    subscription.Bind(&lifecycle);
    ASSERT_EQ(lifecycle.observers.size(), 1u);
    EXPECT_GT(lifecycle.observers.begin()->first, rebound);
    ASSERT_EQ(lifecycle.removed.size(), 2u);
    EXPECT_EQ(lifecycle.removed[0], original);
    EXPECT_EQ(lifecycle.removed[1], rebound);

    subscription.BeginShutdown();
    EXPECT_FALSE(subscription.is_bound());
    EXPECT_TRUE(lifecycle.observers.empty());
    EXPECT_EQ(lifecycle.removed.size(), 3u);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, FrameSubscriptionShutdownRemovesOnlyItsOwnId)
{
    MonitorLifecycle lifecycle;
    performance::FrameSubscription subscription(monitor);
    nitroapi::FrameLifecycleSubscription_t independent = lifecycle.Subscribe(&monitor);
    subscription.Bind(&lifecycle);
    ASSERT_EQ(lifecycle.observers.size(), 2u);
    subscription.BeginShutdown();
    monitor.Shutdown();
    EXPECT_FALSE(subscription.is_bound());
    ASSERT_EQ(lifecycle.observers.size(), 1u);
    EXPECT_EQ(lifecycle.observers.begin()->first, independent);
    subscription.BeginShutdown();
    EXPECT_EQ(lifecycle.removed.size(), 1u);
    lifecycle.Unsubscribe(independent);
}

TEST_F(PerformanceMonitorTest, FrameSubscriptionAbortedShutdownClosesTraceBeforeUnsubscribe)
{
    MonitorTrace trace;
    MonitorLifecycle lifecycle;
    performance::FrameSubscription subscription(monitor);
    monitor.SetTraceBackend(&trace);
    subscription.Bind(&lifecycle);
    ASSERT_EQ(lifecycle.observers.size(), 1u);
    nitroapi::FrameLifecycleObserverInterface* observer = lifecycle.observers.begin()->second;
    observer->OnHostEntry();
    observer->OnFrameAccepted();
    lifecycle.on_unsubscribe = [&] {
        EXPECT_FALSE(monitor.is_inside_host());
        EXPECT_EQ(trace.cpu_ends, 1u);
    };
    subscription.BeginShutdown();
    monitor.Shutdown();
    EXPECT_TRUE(subscription.is_bound());
    observer->OnHostExit(false);
    EXPECT_FALSE(subscription.is_bound());
    EXPECT_FALSE(monitor.is_inside_host());
    EXPECT_TRUE(lifecycle.observers.empty());
    EXPECT_EQ(lifecycle.removed.size(), 1u);
    EXPECT_EQ(trace.cpu_begins, trace.cpu_ends);
    monitor.SetTraceBackend(nullptr);
}

TEST_F(PerformanceMonitorTest, FrameSubscriptionDestructionRemovesOnlyItsOwnId)
{
    MonitorLifecycle lifecycle;
    nitroapi::FrameLifecycleSubscription_t independent = lifecycle.Subscribe(&monitor);
    nitroapi::FrameLifecycleSubscription_t owned = 0;

    {
        performance::FrameSubscription subscription(monitor);
        subscription.Bind(&lifecycle);
        ASSERT_EQ(lifecycle.observers.size(), 2u);
        owned = lifecycle.observers.rbegin()->first;
        EXPECT_NE(owned, independent);
    }

    ASSERT_EQ(lifecycle.observers.size(), 1u);
    EXPECT_EQ(lifecycle.observers.begin()->first, independent);
    ASSERT_EQ(lifecycle.removed.size(), 1u);
    EXPECT_EQ(lifecycle.removed[0], owned);
    monitor.SetMode(1);
    EXPECT_EQ(monitor.get_mode(), 1);
    lifecycle.Unsubscribe(independent);
}
