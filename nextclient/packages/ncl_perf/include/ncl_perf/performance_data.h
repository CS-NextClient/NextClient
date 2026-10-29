#pragma once

#include <cstdint>

namespace ncl_perf
{
    using PerfTimestamp_t = uint64_t;
    using PerfDuration_t = uint64_t;
    using PerfScopeToken_t = uint64_t;
    using PerfScopeId_t = uint32_t;

    inline constexpr uint32_t kPerfScopeCapacity = 128;
    inline constexpr uint32_t kPerfNameCapacity = 96;
    inline constexpr uint32_t kPerfStackCapacity = 64;

    enum class PerfSampleState : uint32_t
    {
        Measured,
        Inactive,
        Pending,
        Unavailable,
        Dropped,
        Incomplete
    };

    enum class PerfScopeCapability : uint32_t
    {
        Cpu = 1,
        Gpu = 2,
        CpuGpu = 3
    };

    struct PerfScopeDesc
    {
        PerfScopeId_t scope_id{};
        const char* name{};
        PerfScopeId_t parent_id{};
        PerfScopeCapability capabilities = PerfScopeCapability::Cpu;
    };

    struct PerfCpuSample
    {
        PerfScopeId_t scope_id{};
        PerfScopeId_t parent_id{};
        PerfDuration_t inclusive_ns{};
        PerfDuration_t self_ns{};
        uint64_t calls{};
        PerfSampleState state = PerfSampleState::Inactive;
        char name[kPerfNameCapacity]{};
    };

    class PerfClockInterface
    {
    public:
        virtual ~PerfClockInterface() = default;
        virtual PerfTimestamp_t Now() = 0;
    };
} // namespace ncl_perf
