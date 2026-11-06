#include "performance/TracyGpuAdapter.h"

#include <array>
#include <map>
#include <optional>
#include <tuple>

#include <glad/glad.h>
#include <ncl_perf/profiler.h>
#if USE_PROFILER
#include <tracy/TracyOpenGL.hpp>
#endif

#include "performance/performance_stages.h"
#include "performance/PerformanceMetrics.h"

namespace performance
{
#if USE_PROFILER
    namespace
    {
        class TracySourceLocations
        {
        private:
            using LocationKey_t = std::tuple<ncl_perf::PerfScopeId_t, uintptr_t, uintptr_t, uint32_t>;
            std::map<LocationKey_t, tracy::SourceLocationData> locations_;

        public:
            const tracy::SourceLocationData* EnsureLocation(ncl_perf::PerfScopeId_t scope_id, std::source_location location)
            {
                LocationKey_t key{
                    scope_id,
                    reinterpret_cast<uintptr_t>(location.file_name()),
                    reinterpret_cast<uintptr_t>(location.function_name()),
                    location.line()
                };
                tracy::SourceLocationData source{
                    stages::ScopeName(scope_id), location.function_name(), location.file_name(), location.line(), 0
                };

                // Tracy can request metadata after a zone ends, so descriptors must keep stable addresses.
                return &locations_.try_emplace(key, source).first->second;
            }
        };

        TracySourceLocations g_SourceLocations;

    } // namespace

    struct TracyGpuAdapter::State
    {
        struct Slot
        {
            ncl_perf::PerfScopeToken_t token{};
            std::optional<tracy::GpuCtxScope> scope;
        };
        std::unique_ptr<tracy::GpuCtx> context;
        std::array<Slot, ncl_perf::kPerfScopeCapacity> slots{};
    };

    struct TracyPerformanceBackend::State
    {
        struct Slot
        {
            ncl_perf::PerfScopeToken_t token{};
            TracyCZoneCtx zone{};
        };
        std::array<Slot, ncl_perf::kPerfScopeCapacity> slots{};
    };
#else
    struct TracyGpuAdapter::State
    {};
    struct TracyPerformanceBackend::State
    {};
#endif

    TracyGpuAdapter::TracyGpuAdapter() = default;
    TracyGpuAdapter::~TracyGpuAdapter()
    {
        AbandonGl();
    }
    TracyPerformanceBackend::TracyPerformanceBackend() = default;
    TracyPerformanceBackend::~TracyPerformanceBackend() = default;

    void TracyGpuAdapter::Initialize()
    {
#if USE_PROFILER
        if (!state_ && ncl_perf::Profiling_IsStarted() && tracy::GetGpuCtxCounter().load() < 255)
        {
            if (!(GLAD_GL_VERSION_3_3 || GLAD_GL_ARB_timer_query) || !glGetInteger64v || !glGenQueries || !glDeleteQueries || !glGetQueryiv ||
                !glQueryCounter || !glGetQueryObjectiv || !glGetQueryObjectui64v || !glGetIntegerv || !glGetError || !glGetString ||
                (GLAD_GL_VERSION_3_0 && !glGetStringi))
            {
                return;
            }

            GLint bits = 0;
            glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
            if (bits <= 0)
            {
                return;
            }

            state_ = std::make_unique<State>();
            state_->context = std::make_unique<tracy::GpuCtx>();
            tracy::GetGpuCtx().ptr = state_->context.get();
            state_->context->Name("NextClient renderer", 19);
        }
#endif
    }

    void TracyGpuAdapter::Collect()
    {
#if USE_PROFILER
        if (state_ && ncl_perf::Profiling_IsStarted())
        {
            state_->context->Collect();
        }
#endif
    }

    void TracyGpuAdapter::ReleaseGl()
    {
#if USE_PROFILER
        if (state_)
        {
            for (uint32_t remaining = 0; remaining < state_->slots.size(); ++remaining)
            {
                State::Slot* newest = nullptr;
                for (State::Slot& slot : state_->slots)
                {
                    if (slot.token && (!newest || slot.token > newest->token))
                    {
                        newest = &slot;
                    }
                }
                if (!newest)
                {
                    break;
                }
                if (!ncl_perf::Profiling_IsStarted())
                {
                    newest->scope->Deactivate();
                }
                newest->scope.reset();
                newest->token = 0;
            }
            if (ncl_perf::Profiling_IsStarted())
            {
                state_->context->Collect();
                tracy::GetGpuCtx().ptr = nullptr;
            }
            state_->context->ReleaseGl();
            state_.reset();
        }
#endif
    }

    void TracyGpuAdapter::AbandonGl()
    {
#if USE_PROFILER
        if (state_)
        {
            for (State::Slot& slot : state_->slots)
            {
                if (slot.scope)
                {
                    slot.scope->Deactivate();
                    slot.scope.reset();
                }
            }
            state_->context->AbandonGl();
            if (ncl_perf::Profiling_IsStarted())
            {
                tracy::GetGpuCtx().ptr = nullptr;
            }
            state_.reset();
        }
#endif
    }

    ncl_perf::PerfScopeToken_t TracyGpuAdapter::Begin(ncl_perf::PerfScopeId_t scope_id, std::source_location location)
    {
#if USE_PROFILER
        if (state_ && ncl_perf::Profiling_IsConnected() && scope_id < ncl_perf::kPerfScopeCapacity)
        {
            for (State::Slot& slot : state_->slots)
            {
                if (!slot.token)
                {
                    slot.scope.emplace(g_SourceLocations.EnsureLocation(scope_id, location), true);
                    if (!slot.scope->IsActive())
                    {
                        slot.scope.reset();
                        return 0;
                    }
                    slot.token = ++sequence_;
                    return slot.token;
                }
            }
        }
#endif
        return 0;
    }

    void TracyGpuAdapter::End(ncl_perf::PerfScopeToken_t token)
    {
#if USE_PROFILER
        if (state_ && token)
        {
            for (State::Slot& slot : state_->slots)
            {
                if (slot.token == token)
                {
                    slot.scope.reset();
                    slot.token = 0;
                    return;
                }
            }
        }
#endif
    }

    void TracyPerformanceBackend::InitializeGpu()
    {
        gpu_.Initialize();
    }

    void TracyPerformanceBackend::ReleaseGl()
    {
        gpu_.ReleaseGl();
    }

    void TracyPerformanceBackend::AbandonGl()
    {
        gpu_.AbandonGl();
    }

    bool TracyPerformanceBackend::IsConnected() const
    {
        return ncl_perf::Profiling_IsConnected();
    }

    ncl_perf::PerfScopeToken_t TracyPerformanceBackend::BeginCpu(ncl_perf::PerfScopeId_t scope_id, std::source_location location)
    {
#if USE_PROFILER
        if (!IsConnected() || scope_id >= ncl_perf::kPerfScopeCapacity)
        {
            return 0;
        }
        if (!state_)
        {
            state_ = std::make_unique<State>();
        }
        for (State::Slot& slot : state_->slots)
        {
            if (!slot.token)
            {
                const tracy::SourceLocationData* source = g_SourceLocations.EnsureLocation(scope_id, location);
                slot.zone = ___tracy_emit_zone_begin(reinterpret_cast<const ___tracy_source_location_data*>(source), 1);
                slot.token = ++sequence_;
                return slot.token;
            }
        }
#endif
        return 0;
    }

    void TracyPerformanceBackend::EndCpu(ncl_perf::PerfScopeToken_t token)
    {
#if USE_PROFILER
        if (state_ && token)
        {
            for (State::Slot& slot : state_->slots)
            {
                if (slot.token == token)
                {
                    if (IsConnected())
                    {
                        ___tracy_emit_zone_end(slot.zone);
                    }
                    slot = {};
                    return;
                }
            }
        }
#endif
    }

    ncl_perf::PerfScopeToken_t TracyPerformanceBackend::BeginGpu(ncl_perf::PerfScopeId_t scope_id, std::source_location location)
    {
        return gpu_.Begin(scope_id, location);
    }

    void TracyPerformanceBackend::EndGpu(ncl_perf::PerfScopeToken_t token)
    {
        gpu_.End(token);
    }

    void TracyPerformanceBackend::Collect()
    {
        gpu_.Collect();
    }

    void TracyPerformanceBackend::MarkFrame(bool output, uint64_t id)
    {
#if USE_PROFILER
        if (IsConnected())
        {
            if (output)
            {
                FrameMark;
                FrameMarkNamed("Output");
                NCL_PROFILE_VALUE("Output ID", id);
            }
            else
            {
                FrameMarkNamed("Host");
                NCL_PROFILE_VALUE("Host ID", id);
            }
        }
#endif
    }

    void TracyPerformanceBackend::RecordCounter(PerfCounterId counter, uint64_t value)
    {
        NCL_PROFILE_VALUE(stages::CounterName(counter), value);
    }

    void TracyPerformanceBackend::RecordGauge(PerfGaugeId gauge, double value)
    {
        const performance::GaugeDescriptor* descriptor = performance::Gauge(gauge);

        if (descriptor)
        {
            NCL_PROFILE_VALUE(descriptor->name, value);
        }
    }
} // namespace performance
