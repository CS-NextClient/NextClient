#pragma once
#include <utility>

#include <concurrencpp/concurrencpp.h>

#include "SynchronizationContextImplInterface.h"
#include "ContinuationContextType.h"

namespace taskcoro
{
    class SynchronizationContext
    {
        friend class TaskCoro;

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

        concurrencpp::result<void> SwitchTo();

        static std::shared_ptr<SynchronizationContext> Current();

    private:
        static void SetCurrent(std::shared_ptr<SynchronizationContext> sync_ctx);
    };
}

#include "SynchronizationContext.tpp"
