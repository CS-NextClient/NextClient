#pragma once

#include "performance/PerformanceMonitor.h"

namespace performance
{
    class FrameSubscription : public nitroapi::FrameLifecycleObserverInterface
    {
    private:
        PerformanceMonitor& monitor_;
        nitroapi::FrameLifecycleInterface* lifecycle_{};
        nitroapi::FrameLifecycleSubscription_t subscription_{};
        bool shutting_down_{};

    public:
        explicit FrameSubscription(PerformanceMonitor& monitor) :
            monitor_(monitor)
        {}
        ~FrameSubscription() override
        {
            Reset();
        }
        FrameSubscription(const FrameSubscription&) = delete;
        FrameSubscription& operator=(const FrameSubscription&) = delete;

        void Bind(nitroapi::FrameLifecycleInterface* lifecycle)
        {
            Reset();
            shutting_down_ = false;
            lifecycle_ = lifecycle;

            if (lifecycle_)
            {
                subscription_ = lifecycle_->Subscribe(this);
            }
        }

        void BeginShutdown()
        {
            shutting_down_ = true;

            if (!monitor_.is_inside_host())
            {
                Reset();
            }
        }

        bool is_bound() const
        {
            return subscription_ != 0;
        }

        void OnHostEntry() override
        {
            monitor_.OnHostEntry();
        }

        void OnFrameAccepted() override
        {
            monitor_.OnFrameAccepted();
        }

        void OnHostExit(bool completed) override
        {
            monitor_.OnHostExit(completed);

            if (shutting_down_ && !monitor_.is_inside_host())
            {
                Reset();
            }
        }

    private:
        void Reset()
        {
            if (lifecycle_ && subscription_)
            {
                lifecycle_->Unsubscribe(subscription_);
            }

            subscription_ = 0;
            lifecycle_ = nullptr;
        }
    };
} // namespace performance
