#pragma once
#include <functional>
#include <mutex>
#include <queue>

#include "../SynchronizationContextImplInterface.h"

namespace taskcoro
{
    class SynchronizationContextManual : public SynchronizationContextImplInterface
    {
        std::queue<std::function<void()>> callbacks_queue_{};
        std::recursive_mutex mutex_{};

    public:
        void RunTask(std::function<void()> task) override;

        void Update();
    };
}
