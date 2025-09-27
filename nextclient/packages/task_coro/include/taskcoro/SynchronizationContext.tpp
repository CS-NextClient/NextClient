#pragma once

namespace taskcoro
{
    template<class TCallable, class... TArgs>
    void SynchronizationContext::Run(TCallable&& callable, TArgs&&... arguments)
    {
        auto bound_call = [func = std::decay_t<TCallable>(std::forward<TCallable>(callable)),
                           ...xs = std::unwrap_ref_decay_t<TArgs>(std::forward<TArgs>(arguments))]
        () mutable
        {
            std::invoke(std::move(func), std::forward_like<TArgs>(xs)...);
        };

        sync_ctx_impl_->RunTask(std::move(bound_call));
    }
}
