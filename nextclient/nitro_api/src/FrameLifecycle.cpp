#include "FrameLifecycle.h"

#include <algorithm>
#include <exception>
#include <limits>

namespace nitroapi
{
    namespace
    {
        FrameLifecycleService g_DefaultService;
    } // namespace

    FrameLifecycleSubscription_t FrameLifecycleService::Subscribe(FrameLifecycleObserverInterface* observer)
    {
        if (!observer)
        {
            return 0;
        }

        if (last_subscription_ == std::numeric_limits<FrameLifecycleSubscription_t>::max())
        {
            std::terminate();
        }

        FrameLifecycleSubscription_t subscription = ++last_subscription_;
        registrations_.push_back({subscription, observer});

        return subscription;
    }

    void FrameLifecycleService::Unsubscribe(FrameLifecycleSubscription_t subscription)
    {
        std::erase_if(registrations_, [subscription](const Registration& registration) {
            return registration.subscription == subscription;
        });
    }

    void FrameLifecycleService::Clear()
    {
        registrations_.clear();
    }

    FrameLifecycleService::InvocationFrame& FrameLifecycleService::AcquireFrame()
    {
        if (leased_frames_ == frames_.size())
        {
            frames_.emplace_back();
        }

        InvocationFrame& frame = frames_[leased_frames_];
        frame.recipients.clear();

        if (registrations_.size() > frame.recipients.capacity())
        {
            frame.recipients.reserve(registrations_.size());
        }

        for (const Registration& registration : registrations_)
        {
            frame.recipients.push_back({registration.subscription});
        }

        ++leased_frames_;
        return frame;
    }

    FrameLifecycleObserverInterface* FrameLifecycleService::FindObserver(FrameLifecycleSubscription_t subscription) const
    {
        auto registration = std::find_if(registrations_.begin(), registrations_.end(), [subscription](const Registration& candidate) {
            return candidate.subscription == subscription;
        });

        return registration != registrations_.end() ? registration->observer : nullptr;
    }

    FrameLifecycleInvocation::FrameLifecycleInvocation(FrameLifecycleService& service) :
        service_(service),
        previous_(service.invocation_),
        frame_(service.AcquireFrame())
    {
        service_.invocation_ = this;

        for (size_t index = 0; index < frame_.recipients.size(); ++index)
        {
            FrameLifecycleObserverInterface* observer = service_.FindObserver(frame_.recipients[index].subscription);

            if (observer)
            {
                frame_.recipients[index].entered = true;
                observer->OnHostEntry();
            }

            if (accepted_)
            {
                NotifyAccepted();
            }
        }
    }

    FrameLifecycleInvocation::~FrameLifecycleInvocation()
    {
        service_.invocation_ = previous_;

        for (size_t index = frame_.recipients.size(); index > 0; --index)
        {
            if (!frame_.recipients[index - 1].entered)
            {
                continue;
            }

            FrameLifecycleObserverInterface* observer = service_.FindObserver(frame_.recipients[index - 1].subscription);

            if (observer)
            {
                observer->OnHostExit(body_depth_ == 0);
            }
        }

        // Exit callbacks can enter Host after the current invocation has been restored.
        --service_.leased_frames_;
    }

    void FrameLifecycleInvocation::ReportFilter(int result)
    {
        if (!result || accepted_)
        {
            return;
        }

        accepted_ = true;
        NotifyAccepted();
    }

    void FrameLifecycleInvocation::NotifyAccepted()
    {
        for (size_t index = 0; index < frame_.recipients.size(); ++index)
        {
            if (!frame_.recipients[index].entered || frame_.recipients[index].accepted)
            {
                continue;
            }

            FrameLifecycleObserverInterface* observer = service_.FindObserver(frame_.recipients[index].subscription);

            if (observer)
            {
                frame_.recipients[index].accepted = true;
                observer->OnFrameAccepted();
            }
        }
    }

    void FrameLifecycleInvocation::BeginBody()
    {
        ++body_depth_;
    }

    void FrameLifecycleInvocation::EndBody()
    {
        --body_depth_;
    }

    void FrameLifecycleService::ReportFilter(int result)
    {
        if (invocation_)
        {
            invocation_->ReportFilter(result);
        }
    }

    void FrameLifecycleService::BeginBody()
    {
        if (invocation_)
        {
            invocation_->BeginBody();
        }
    }

    void FrameLifecycleService::EndBody()
    {
        if (invocation_)
        {
            invocation_->EndBody();
        }
    }

    FrameLifecycleService& FrameLifecycle_GetDefaultService()
    {
        return g_DefaultService;
    }

    FrameLifecycleInterface* FrameLifecycle_GetInterface()
    {
        return &FrameLifecycle_GetDefaultService();
    }

    void FrameLifecycle_ClearObservers()
    {
        FrameLifecycle_GetDefaultService().Clear();
    }
} // namespace nitroapi
