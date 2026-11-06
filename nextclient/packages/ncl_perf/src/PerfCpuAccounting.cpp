#include <ncl_perf/PerfCpuAccounting.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace ncl_perf
{
    PerfCpuAccounting::PerfCpuAccounting(PerfClockInterface& clock) :
        clock_(clock)
    {}

    PerfScopeId_t PerfCpuAccounting::RegisterScope(const PerfScopeDesc& descriptor)
    {
        if (!descriptor.scope_id || !descriptor.name || !descriptor.name[0] || registry_count_ == registry_.size() || stack_count_)
        {
            return 0;
        }

        uint32_t length = 0;
        while (length < kPerfNameCapacity && descriptor.name[length])
        {
            ++length;
        }

        if (length == kPerfNameCapacity || FindScope(descriptor.scope_id) < registry_count_)
        {
            return 0;
        }

        uint32_t index = 0;
        while (index < registry_count_ && registry_[index].sample.scope_id < descriptor.scope_id)
        {
            ++index;
        }

        for (uint32_t moved = registry_count_; moved > index; --moved)
        {
            registry_[moved] = registry_[moved - 1];
        }

        registry_[index] = {};
        RegisteredScope& entry = registry_[index];
        entry.sample.scope_id = descriptor.scope_id;
        entry.sample.parent_id = descriptor.parent_id;
        std::memcpy(entry.sample.name, descriptor.name, length + 1);
        entry.capabilities = descriptor.capabilities;

        if ((uint32_t(descriptor.capabilities) & uint32_t(PerfScopeCapability::Cpu)) == 0)
        {
            entry.sample.state = PerfSampleState::Unavailable;
        }

        ++registry_count_;

        return descriptor.scope_id;
    }

    PerfScopeToken_t PerfCpuAccounting::Begin(PerfScopeId_t scope_id)
    {
        if (!enabled_)
        {
            return 0;
        }

        uint32_t index = FindScope(scope_id);

        if (index == registry_count_ || (uint32_t(registry_[index].capabilities) & uint32_t(PerfScopeCapability::Cpu)) == 0)
        {
            return 0;
        }

        if (stack_count_ == stack_.size())
        {
            incomplete_ = true;
            ++dropped_scopes_;
            return 0;
        }

        // Sequence exhaustion must not make a stale token identify a new scope.
        if (++sequence_ == 0)
        {
            std::abort();
        }

        RegisteredScope& entry = registry_[index];
        PerfScopeId_t parent = stack_count_ ? registry_[stack_[stack_count_ - 1].registry_index].sample.scope_id : 0;

        if (!entry.parent_seen)
        {
            entry.sample.parent_id = parent;
            entry.parent_seen = true;
        }
        else if (entry.sample.parent_id != parent)
        {
            entry.sample.parent_id = 0;
        }

        PerfScopeToken_t token = (uint64_t(generation_) << 32) | sequence_;
        stack_[stack_count_++] = {token, clock_.Now(), 0, index};

        return token;
    }

    void PerfCpuAccounting::End(PerfScopeToken_t token)
    {
        if (!token || uint32_t(token >> 32) != generation_ || !enabled_)
        {
            return;
        }

        if (!stack_count_ || stack_[stack_count_ - 1].token != token)
        {
            std::abort();
        }

        ActiveScope active = stack_[--stack_count_];
        PerfTimestamp_t now = clock_.Now();

        if (now < active.start)
        {
            std::abort();
        }

        PerfDuration_t duration = now - active.start;
        PerfCpuSample& sample = registry_[active.registry_index].sample;

        last_scope_id_ = sample.scope_id;
        last_duration_ns_ = duration;
        sample.inclusive_ns += duration;
        sample.self_ns += duration - active.children;
        ++sample.calls;
        sample.state = PerfSampleState::Measured;

        if (stack_count_)
        {
            stack_[stack_count_ - 1].children += duration;
        }
    }

    uint32_t PerfCpuAccounting::CopySamples(PerfCpuSample* output, uint32_t capacity) const
    {
        uint32_t count = std::min(capacity, registry_count_);

        for (uint32_t index = 0; index < count; ++index)
        {
            output[index] = registry_[index].sample;
            if (incomplete_ && output[index].calls)
            {
                output[index].state = PerfSampleState::Incomplete;
                output[index].self_ns = 0;
            }
        }

        return count;
    }

    void PerfCpuAccounting::ClearFrame()
    {
        if (stack_count_)
        {
            std::abort();
        }

        for (uint32_t index = 0; index < registry_count_; ++index)
        {
            PerfCpuSample& sample = registry_[index].sample;

            registry_[index].parent_seen = false;
            sample.parent_id = 0;
            sample.inclusive_ns = 0;
            sample.self_ns = 0;
            sample.calls = 0;
            sample.state = (uint32_t(registry_[index].capabilities) & uint32_t(PerfScopeCapability::Cpu)) == 0 ? PerfSampleState::Unavailable
                                                                                                               : PerfSampleState::Inactive;
        }

        incomplete_ = false;
        dropped_scopes_ = 0;
    }

    uint64_t PerfCpuAccounting::dropped_scopes() const
    {
        return dropped_scopes_;
    }

    void PerfCpuAccounting::Reset(uint32_t generation)
    {
        last_scope_id_ = 0;
        last_duration_ns_ = 0;

        if (generation_ != generation)
        {
            sequence_ = 0;
        }

        generation_ = generation;
        stack_count_ = 0;
        ClearFrame();
    }

    void PerfCpuAccounting::SetEnabled(bool enabled)
    {
        if (!enabled && enabled_)
        {
            Reset(generation_ + 1);
        }

        enabled_ = enabled;
    }

    uint32_t PerfCpuAccounting::FindScope(PerfScopeId_t scope_id) const
    {
        uint32_t first = 0;
        uint32_t last = registry_count_;

        while (first < last)
        {
            uint32_t middle = first + (last - first) / 2;

            if (registry_[middle].sample.scope_id < scope_id)
            {
                first = middle + 1;
            }
            else
            {
                last = middle;
            }
        }

        return first < registry_count_ && registry_[first].sample.scope_id == scope_id ? first : registry_count_;
    }

    PerfScopeId_t PerfCpuAccounting::last_scope_id() const
    {
        return last_scope_id_;
    }

    PerfDuration_t PerfCpuAccounting::last_duration_ns() const
    {
        return last_duration_ns_;
    }
} // namespace ncl_perf
