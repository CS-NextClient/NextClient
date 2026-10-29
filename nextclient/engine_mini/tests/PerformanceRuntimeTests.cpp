#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <memory>

#include "RendererOwnerTestSupport.h"
#include "PerformanceRuntimeTestSupport.h"
#include "chain/HandlersChain.h"
#include "performance/PerformanceRuntime.h"
#include "performance/PerformanceTraceBackend.h"
#ifdef _WIN32
#include "graphics/text/text_render.h"
#endif

namespace
{
    using performance_test::RuntimeHook;

    class RuntimeClock final : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t time = 1000000000;
        uint32_t reads{};

        ncl_perf::PerfTimestamp_t Now() override
        {
            ++reads;
            return time;
        }
    };

    class RuntimeLifecycle final : public nitroapi::FrameLifecycleInterface
    {
    private:
        nitroapi::FrameLifecycleSubscription_t next_{};

    public:
        std::map<nitroapi::FrameLifecycleSubscription_t, nitroapi::FrameLifecycleObserverInterface*> observers;
        uint32_t detached{};

        nitroapi::FrameLifecycleSubscription_t Subscribe(nitroapi::FrameLifecycleObserverInterface* observer) override
        {
            observers.emplace(++next_, observer);
            return next_;
        }

        void Unsubscribe(nitroapi::FrameLifecycleSubscription_t subscription) override
        {
            detached += static_cast<uint32_t>(observers.erase(subscription));
        }
    };

    class RuntimeTrace final : public performance::PerformanceTraceBackendInterface
    {
    public:
        uint32_t begun{};
        uint32_t ended{};
        uint32_t completed{};
        bool IsConnected() const override
        {
            return true;
        }
        ncl_perf::PerfScopeToken_t BeginCpu(ncl_perf::PerfScopeId_t, std::source_location) override
        {
            return ++begun;
        }
        void EndCpu(ncl_perf::PerfScopeToken_t) override
        {
            ++ended;
        }
        ncl_perf::PerfScopeToken_t BeginGpu(ncl_perf::PerfScopeId_t, std::source_location) override
        {
            return 0;
        }
        void EndGpu(ncl_perf::PerfScopeToken_t) override {}
        void Collect() override {}
        void RecordCounter(performance::PerfCounterId, uint64_t) override {}
        void RecordGauge(performance::PerfGaugeId, double) override {}
        void MarkFrame(bool, uint64_t) override
        {
            ++completed;
        }
    };

    struct RuntimeOwners
    {
        RuntimeClock clock;
        performance::PerformanceMonitor monitor{clock};
        performance::PerformanceMapper mapper;
        renderer_test::Owners renderer;
        nitroapi::EngineData engine;
        nitroapi::ClientData client;
        RuntimeLifecycle lifecycle;
        performance::PerformanceRuntime runtime;

        explicit RuntimeOwners(bool frame_callbacks = true) :
            runtime(
                engine,
                client,
                nullptr,
                frame_callbacks ? &lifecycle : nullptr,
                monitor,
                mapper,
                clock,
                renderer.resources.device,
                renderer.state.lifecycle,
#ifdef _WIN32
                TextRender_Get(),
#else
                nullptr,
#endif
                8684
            )
        {}
    };
} // namespace

TEST(PerformanceRuntimeOwner, HooksUseSuppliedEngineClientAndMonitorThenDetach)
{
    nitroapi::HandlersChain<void> engine_chain;
    RuntimeHook<void> engine_hook;
    nitroapi::HandlersChain<int, float, int> client_chain;
    RuntimeHook<int, float, int> client_hook;
    std::unique_ptr<RuntimeOwners> first = std::make_unique<RuntimeOwners>();
    std::unique_ptr<RuntimeOwners> second = std::make_unique<RuntimeOwners>();
    first->engine.S_ExtraUpdate.SetChain(&engine_chain);
    first->engine.S_ExtraUpdate.SetHook(&engine_hook);
    first->client.HUD_Redraw.SetChain(&client_chain);
    first->client.HUD_Redraw.SetHook(&client_hook);
    engine_hook.original = [&] {
        first->clock.time += 10;
    };
    client_hook.original = [&](float time, int intermission) {
        EXPECT_FLOAT_EQ(time, 0.5f);
        EXPECT_EQ(intermission, 2);
        first->clock.time += 20;
        return 42;
    };
    first->runtime.Bind();
    second->runtime.Bind();
    EXPECT_EQ(engine_chain.GetHandlersCount(), 1u);
    EXPECT_EQ(client_chain.GetHandlersCount(), 1u);
    first->monitor.SetMode(1);
    first->monitor.OnHostEntry();
    first->monitor.OnFrameAccepted();
    engine_hook.Invoke();
    EXPECT_EQ(client_hook.Invoke(0.5f, 2), 42);
    first->monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    first->monitor.GetSnapshot(snapshot);
    uint32_t measured = 0;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        const ncl_perf::PerfCpuSample& sample = snapshot.cpu_tree[index];
        if (sample.scope_id == performance::PerfStage::SoundExtra || sample.scope_id == performance::PerfStage::Hud)
        {
            ++measured;
            EXPECT_EQ(sample.calls, 1u);
            EXPECT_EQ(sample.state, ncl_perf::PerfSampleState::Measured);
            EXPECT_EQ(sample.inclusive_ns, sample.scope_id == performance::PerfStage::SoundExtra ? 10u : 20u);
        }
    }
    EXPECT_EQ(measured, 2u);
    EXPECT_EQ(second->monitor.get_mode(), 0);
    first->runtime.Shutdown();
    EXPECT_EQ(engine_chain.GetHandlersCount(), 0u);
    EXPECT_EQ(client_chain.GetHandlersCount(), 0u);
    EXPECT_EQ(client_hook.Invoke(0.5f, 2), 42);
    second->runtime.Shutdown();
    first->engine.S_ExtraUpdate.SetChain(nullptr);
    first->client.HUD_Redraw.SetChain(nullptr);
}

TEST(PerformanceRuntimeOwner, SuppliedMonitorAndMapperRemainIndependent)
{
    std::unique_ptr<RuntimeOwners> first = std::make_unique<RuntimeOwners>();
    std::unique_ptr<RuntimeOwners> second = std::make_unique<RuntimeOwners>();
    goldsrc::cvar_t mode{};
    mode.value = 2;
    first->runtime.OnModeChanged(&mode);
    first->runtime.OnMapperChanged(&mode);
    EXPECT_EQ(first->monitor.get_mode(), 2);
    EXPECT_TRUE(first->mapper.is_enabled());
    EXPECT_EQ(second->monitor.get_mode(), 0);
    EXPECT_FALSE(second->mapper.is_enabled());
    uint32_t generation = second->monitor.map_generation();
    first->runtime.MapChanged();
    EXPECT_GT(first->monitor.map_generation(), generation);
    EXPECT_EQ(second->monitor.map_generation(), generation);
    first->runtime.Shutdown();
    second->runtime.Shutdown();
}

TEST(PerformanceRuntimeOwner, NetworkReadsCurrentPointeesFromItsSuppliedTable)
{
    std::unique_ptr<RuntimeOwners> first = std::make_unique<RuntimeOwners>();
    std::unique_ptr<RuntimeOwners> second = std::make_unique<RuntimeOwners>();
    std::unique_ptr<goldsrc::client_static_t> active = std::make_unique<goldsrc::client_static_t>();
    std::unique_ptr<goldsrc::client_state_t> client_state = std::make_unique<goldsrc::client_state_t>();
    double realtime = 10;
    active->state = goldsrc::ca_active;
    active->netchan.connect_time = 1;
    first->engine.client_static = active.get();
    first->engine.client_state = client_state.get();
    second->engine.client_state = client_state.get();
    first->engine.realtime = &realtime;
    second->engine.realtime = &realtime;
    goldsrc::cvar_t enabled{};
    enabled.value = 1;
    first->runtime.OnNetworkChanged(&enabled);
    second->runtime.OnNetworkChanged(&enabled);
    first->runtime.UpdateNetwork();
    second->runtime.UpdateNetwork();
    EXPECT_TRUE(first->runtime.get_network_snapshot().connected);
    EXPECT_FALSE(second->runtime.get_network_snapshot().connected);
    EXPECT_GT(first->clock.reads, 0u);

    first->engine.client_static = nullptr;
    second->engine.client_static = active.get();
    first->clock.time += 100000000;
    second->clock.time += 100000000;
    first->runtime.UpdateNetwork();
    second->runtime.UpdateNetwork();
    EXPECT_FALSE(first->runtime.get_network_snapshot().connected);
    EXPECT_TRUE(second->runtime.get_network_snapshot().connected);
    first->runtime.Shutdown();
    second->runtime.Shutdown();
}

TEST(PerformanceRuntimeOwner, ActualHookChainReturnsSafelyAfterShutdownUnsubscribesItsExecutingHandler)
{
    nitroapi::HandlersChain<void, goldsrc::qboolean> chain;
    RuntimeHook<void, goldsrc::qboolean> hook;
    RuntimeTrace trace;
    std::unique_ptr<RuntimeOwners> owners = std::make_unique<RuntimeOwners>();
    owners->engine.CL_ParseServerMessage.SetChain(&chain);
    owners->engine.CL_ParseServerMessage.SetHook(&hook);
    uint32_t original_calls = 0;
    hook.original = [&](goldsrc::qboolean normal_message) {
        EXPECT_EQ(normal_message, 1);
        ++original_calls;
        owners->runtime.Shutdown();
        owners->engine.client_static = nullptr;
        owners->engine.client_state = nullptr;
        owners->engine.realtime = nullptr;
    };
    owners->runtime.Bind();
    owners->monitor.SetTraceBackend(&trace);
    goldsrc::cvar_t enabled{};
    enabled.value = 1;
    owners->runtime.OnNetworkChanged(&enabled);
    ASSERT_EQ(owners->lifecycle.observers.size(), 1u);
    nitroapi::FrameLifecycleObserverInterface* observer = owners->lifecycle.observers.begin()->second;
    observer->OnHostEntry();
    observer->OnFrameAccepted();
    hook.Invoke(1);
    EXPECT_EQ(original_calls, 1u);
    EXPECT_EQ(chain.GetHandlersCount(), 0u);
    EXPECT_TRUE(owners->monitor.is_inside_host());
    EXPECT_EQ(owners->lifecycle.detached, 0u);
    observer->OnHostExit(false);
    EXPECT_EQ(owners->lifecycle.detached, 1u);
    EXPECT_EQ(trace.begun, trace.ended);
    EXPECT_EQ(trace.completed, 0u);
    owners->monitor.SetTraceBackend(nullptr);
    owners->engine.CL_ParseServerMessage.SetChain(nullptr);
}

TEST(PerformanceRuntimeOwner, AbsentNativeFunctionsAndOptionalInterfacesStayUnbound)
{
    std::unique_ptr<RuntimeOwners> owners = std::make_unique<RuntimeOwners>(false);
    owners->runtime.Bind();
    EXPECT_TRUE(owners->lifecycle.observers.empty());
    owners->runtime.ContextCreated();
    owners->runtime.ContextChanging();
    owners->runtime.Shutdown();
    owners->runtime.Shutdown();
    EXPECT_EQ(owners->monitor.BeginCpu(performance::PerfStage::Client), 0u);
    EXPECT_EQ(owners->lifecycle.detached, 0u);
}

TEST(PerformanceRuntimeOwner, RenderViewRejectsChangedSessionBeforeCapturingAValidOldWorld)
{
    for (int transition = 0; transition < 5; ++transition)
    {
        SCOPED_TRACE(transition);
        nitroapi::HandlersChain<void> chain;
        RuntimeHook<void> hook;
        std::unique_ptr<RuntimeOwners> owners = std::make_unique<RuntimeOwners>();
        performance_test::MapperInputs inputs(owners->engine);
        performance_test::MapperWorld replacement;
        std::unique_ptr<goldsrc::client_state_t> replacement_client = std::make_unique<goldsrc::client_state_t>();
        replacement_client->worldmodel = &inputs.world.model;
        owners->engine.R_RenderView.SetChain(&chain);
        owners->engine.R_RenderView.SetHook(&hook);
        hook.original = [] {};
        owners->runtime.Bind();
        inputs.Enable(owners->runtime);
        hook.Invoke();
        ASSERT_TRUE(owners->mapper.get_snapshot().view_measured);
        owners->mapper.BeginFrame();
        hook.original = [&] {
            switch (transition)
            {
                case 0:
                    owners->runtime.MapChanged();
                    break;
                case 1:
                    inputs.client->worldmodel = &replacement.model;
                    break;
                case 2:
                    owners->engine.client_state = replacement_client.get();
                    break;
                case 3:
                    inputs.session->netchan.connect_time = 2;
                    break;
                case 4:
                    inputs.session->state = goldsrc::ca_disconnected;
                    break;
            }
        };
        hook.Invoke();
        EXPECT_FALSE(owners->mapper.get_snapshot().view_measured);
        EXPECT_FALSE(owners->mapper.get_snapshot().map_loaded);
        owners->runtime.Shutdown();
        owners->engine.R_RenderView.SetChain(nullptr);
    }
}

TEST(PerformanceRuntimeOwner, ClientStateClearInvalidatesMapperBeforeCallingTheStockBoundary)
{
    nitroapi::HandlersChain<void, goldsrc::qboolean> chain;
    RuntimeHook<void, goldsrc::qboolean> hook;
    std::unique_ptr<RuntimeOwners> owners = std::make_unique<RuntimeOwners>();
    performance_test::MapperInputs inputs(owners->engine);
    owners->engine.CL_ClearState.SetChain(&chain);
    owners->engine.CL_ClearState.SetHook(&hook);
    uint32_t forwarded = 0;
    hook.original = [&](goldsrc::qboolean quiet) {
        EXPECT_EQ(quiet, 1);
        ++forwarded;
        EXPECT_FALSE(owners->mapper.get_snapshot().map_loaded);
    };
    owners->runtime.Bind();
    inputs.Enable(owners->runtime);
    owners->runtime.UpdateMapper();
    ASSERT_TRUE(owners->mapper.get_snapshot().map_loaded);
    uint32_t generation = owners->monitor.map_generation();
    hook.Invoke(1);
    EXPECT_EQ(forwarded, 1u);
    EXPECT_FALSE(owners->mapper.get_snapshot().map_loaded);
    EXPECT_TRUE(owners->mapper.is_enabled());
    EXPECT_EQ(owners->monitor.map_generation(), generation);
    owners->runtime.Shutdown();
    owners->engine.CL_ClearState.SetChain(nullptr);
}

TEST(PerformanceRuntimeOwner, NestedViewAfterClientStateClearKeepsItsSnapshotAndRejectsTheOuterContinuation)
{
    nitroapi::HandlersChain<void> view_chain;
    RuntimeHook<void> view_hook;
    nitroapi::HandlersChain<void, goldsrc::qboolean> clear_chain;
    RuntimeHook<void, goldsrc::qboolean> clear_hook;
    std::unique_ptr<RuntimeOwners> owners = std::make_unique<RuntimeOwners>();
    performance_test::MapperInputs inputs(owners->engine);
    owners->engine.R_RenderView.SetChain(&view_chain);
    owners->engine.R_RenderView.SetHook(&view_hook);
    owners->engine.CL_ClearState.SetChain(&clear_chain);
    owners->engine.CL_ClearState.SetHook(&clear_hook);
    bool nested = false;
    uint64_t nested_revision = 0;
    clear_hook.original = [](goldsrc::qboolean) {};
    view_hook.original = [&] {
        if (nested)
        {
            inputs.wpoly = 123;
            inputs.epoly = 45;
            return;
        }
        clear_hook.Invoke(1);
        nested = true;
        view_hook.Invoke();
        nested = false;
        ASSERT_TRUE(owners->mapper.get_snapshot().view_measured);
        nested_revision = owners->mapper.get_snapshot().revision;
        inputs.wpoly = 777;
        inputs.epoly = 888;
    };
    owners->runtime.Bind();
    inputs.Enable(owners->runtime);
    view_hook.Invoke();
    EXPECT_TRUE(owners->mapper.get_snapshot().map_loaded);
    EXPECT_TRUE(owners->mapper.get_snapshot().view_measured);
    EXPECT_EQ(owners->mapper.get_snapshot().wpoly, 123u);
    EXPECT_EQ(owners->mapper.get_snapshot().epoly, 45u);
    EXPECT_EQ(owners->mapper.get_snapshot().revision, nested_revision);
    owners->runtime.Shutdown();
    owners->engine.R_RenderView.SetChain(nullptr);
    owners->engine.CL_ClearState.SetChain(nullptr);
}

TEST(PerformanceRuntimeOwner, ShutdownKeepsObserverAndTraceUntilNestedOuterHostExit)
{
    RuntimeTrace trace;
    std::unique_ptr<RuntimeOwners> owners = std::make_unique<RuntimeOwners>();
    owners->runtime.Bind();
    owners->monitor.SetTraceBackend(&trace);
    ASSERT_EQ(owners->lifecycle.observers.size(), 1u);
    nitroapi::FrameLifecycleObserverInterface* observer = owners->lifecycle.observers.begin()->second;
    observer->OnHostEntry();
    observer->OnFrameAccepted();
    observer->OnHostEntry();
    owners->runtime.Shutdown();
    EXPECT_TRUE(owners->monitor.is_inside_host());
    EXPECT_EQ(trace.ended, 0u);
    EXPECT_EQ(owners->lifecycle.detached, 0u);
    observer->OnHostExit(true);
    EXPECT_EQ(owners->lifecycle.detached, 0u);
    observer->OnHostExit(true);
    EXPECT_FALSE(owners->monitor.is_inside_host());
    EXPECT_TRUE(owners->lifecycle.observers.empty());
    EXPECT_EQ(owners->lifecycle.detached, 1u);
    EXPECT_EQ(trace.begun, trace.ended);
    EXPECT_EQ(trace.completed, 1u);
    owners->monitor.SetTraceBackend(nullptr);
}

TEST(PerformanceRuntimeOwner, AbortedNestedHostDetachesOnlyAfterOuterExitWithoutCompletedFrame)
{
    RuntimeTrace trace;
    std::unique_ptr<RuntimeOwners> owners = std::make_unique<RuntimeOwners>();
    owners->runtime.Bind();
    owners->monitor.SetTraceBackend(&trace);
    ASSERT_EQ(owners->lifecycle.observers.size(), 1u);
    nitroapi::FrameLifecycleObserverInterface* observer = owners->lifecycle.observers.begin()->second;
    observer->OnHostEntry();
    observer->OnFrameAccepted();
    observer->OnHostEntry();
    owners->runtime.Shutdown();
    observer->OnHostExit(false);
    EXPECT_TRUE(owners->monitor.is_inside_host());
    EXPECT_EQ(owners->lifecycle.detached, 0u);
    observer->OnHostExit(true);
    EXPECT_FALSE(owners->monitor.is_inside_host());
    EXPECT_EQ(owners->lifecycle.detached, 1u);
    EXPECT_EQ(trace.begun, trace.ended);
    EXPECT_EQ(trace.completed, 0u);
    owners->monitor.SetTraceBackend(nullptr);
}
