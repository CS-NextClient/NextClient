#pragma once
#include "catalog.h"
#include <atomic>
#include <utility>

namespace plugins::runtime
{
    inline constexpr size_t memory_limit = 32 * 1024 * 1024;
    inline std::atomic<size_t> memory_used{}, memory_peak{}, memory_denied{};
    // Conservative accounting for host containers, including capacity, nodes and
    // allocator overhead. This is not process RSS and cannot cover DLL allocations.
    class Budget
    {
        size_t bytes_{};

    public:
        Budget() = default;
        explicit Budget(size_t bytes, bool control = false)
        {
            resize(bytes, control);
        }
        Budget(const Budget&) = delete;
        Budget& operator=(const Budget&) = delete;
        Budget(Budget&& other) noexcept :
            bytes_(std::exchange(other.bytes_, 0))
        {}
        Budget& operator=(Budget&& other) noexcept
        {
            memory_used.fetch_sub(bytes_);
            bytes_ = std::exchange(other.bytes_, 0);
            return *this;
        }
        ~Budget()
        {
            memory_used.fetch_sub(bytes_);
        }
        size_t size() const
        {
            return bytes_;
        }
        void absorb(Budget&& other)
        {
            bytes_ += std::exchange(other.bytes_, 0);
        }
        void resize(size_t bytes, bool control = false)
        {
            if (bytes <= bytes_)
                memory_used.fetch_sub(bytes_ - bytes);
            else
            {
                auto used = memory_used.load();
                const auto ceiling = control ? memory_limit : memory_limit - 2 * 1024 * 1024;
                do
                {
                    if (used >= ceiling || bytes - bytes_ > ceiling - used)
                    {
                        ++memory_denied;
                        throw std::runtime_error("Global host resource budget exhausted");
                    }
                } while (!memory_used.compare_exchange_weak(used, used + bytes - bytes_));
                auto peak = memory_peak.load();
                while (peak < used + bytes - bytes_ && !memory_peak.compare_exchange_weak(peak, used + bytes - bytes_))
                {
                }
            }
            bytes_ = bytes;
        }
    };
    inline size_t string_memory(const std::string& s)
    {
        return 2 * (s.capacity() + 1) + 64;
    }
    inline size_t json_memory(const Json& value)
    {
        size_t bytes = 2 * sizeof(Json) + 64;
        if (value.is_string())
            bytes += string_memory(value.get_string());
        else if (value.is_array())
        {
            bytes += value.get_array().capacity() * sizeof(Json);
            for (const auto& child : value.get_array())
                bytes += json_memory(child);
        }
        else if (value.is_object())
            for (const auto& [key, child] : value.get_object())
                bytes += 128 + string_memory(key) + json_memory(child);
        return bytes;
    }
    inline void replace_json(Json& target, Budget& budget, Json next)
    {
        budget.resize(json_memory(next));
        target = std::move(next);
    }
} // namespace plugins::runtime
