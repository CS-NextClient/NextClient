#pragma once
#include <coroutine>
#include <memory>
#include <utility>

#include <concurrencpp/concurrencpp.h>

#include "SynchronizationContextImplInterface.h"
#include "ContinuationContextType.h"

namespace taskcoro
{
    // await_ready must return false whenever a switch is needed (non-null impl): the coroutine
    // has to be suspended before the handle is posted, otherwise the target thread could resume
    // it before suspension completes, leaving the continuation on the source thread (lost switch).
    // A null impl means we are already on the target context, so resume inline.
    struct SwitchToAwaiter
    {
        std::shared_ptr<SynchronizationContextImplInterface> sync_ctx_impl;

        bool await_ready() const noexcept { return sync_ctx_impl == nullptr; }
        void await_suspend(std::coroutine_handle<> handle) const { sync_ctx_impl->RunTask([handle] { handle.resume(); }); }
        void await_resume() const noexcept {}
    };

    class SynchronizationContext
    {
        std::shared_ptr<SynchronizationContextImplInterface> sync_ctx_impl_;

        thread_local static std::shared_ptr<SynchronizationContext> current_;

    public:
        SynchronizationContext() = delete;
        SynchronizationContext(SynchronizationContext&) = delete;
        SynchronizationContext& operator=(SynchronizationContext&) = delete;

        explicit SynchronizationContext(
            std::shared_ptr<SynchronizationContextImplInterface> sync_ctx_impl
        );

        template<class TCallable, class... TArgs>
        void Run(TCallable&& callable, TArgs&&... arguments);

        SwitchToAwaiter SwitchTo();

        static std::shared_ptr<SynchronizationContext> Current();
        static void SetCurrent(std::shared_ptr<SynchronizationContext> sync_ctx);
    };
}

#include "SynchronizationContext.tpp"
