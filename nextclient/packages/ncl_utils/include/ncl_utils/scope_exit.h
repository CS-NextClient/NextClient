#pragma once
#include <utility>
#include <type_traits>

namespace ncl_utils
{
    template <class TCallable>
    class ScopeExit
    {
        TCallable callable_;
        bool is_active_;

    public:
        explicit ScopeExit(TCallable&& callable) noexcept(std::is_nothrow_move_constructible_v<TCallable>) :
            callable_(std::forward<TCallable>(callable)),
            is_active_(true)
        {

        }

        ScopeExit(ScopeExit&& other) noexcept(std::is_nothrow_move_constructible_v<TCallable>) :
            callable_(std::move(other.callable_)),
            is_active_(other.is_active_)
        {
            other.Release();
        }

        ScopeExit(const ScopeExit&) = delete;
        ScopeExit& operator=(const ScopeExit&) = delete;

        ~ScopeExit() noexcept(noexcept(std::declval<TCallable&>()()))
        {
            if (is_active_)
            {
                callable_();
            }
        }

        void Release() noexcept
        {
            is_active_ = false;
        }
    };

    template <class TCallable>
    auto MakeScopeExit(TCallable&& callable)
    {
        return ScopeExit<std::decay_t<TCallable>>(std::forward<TCallable>(callable));
    }
}
