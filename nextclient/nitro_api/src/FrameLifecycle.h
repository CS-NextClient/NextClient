#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

#include <nitroapi/FrameLifecycleInterface.h>

namespace nitroapi
{
    class FrameLifecycleInvocation;
    class FrameLifecycleStorageTestAccess;

    class FrameLifecycleService final : public FrameLifecycleInterface
    {
    private:
        friend class FrameLifecycleInvocation;
        friend class FrameLifecycleStorageTestAccess;

        struct Registration
        {
            FrameLifecycleSubscription_t subscription{};
            FrameLifecycleObserverInterface* observer{};
        };

        struct Recipient
        {
            FrameLifecycleSubscription_t subscription{};
            bool entered{};
            bool accepted{};
        };

        struct InvocationFrame
        {
            std::vector<Recipient> recipients;
        };

        std::vector<Registration> registrations_;
        FrameLifecycleSubscription_t last_subscription_{};
        FrameLifecycleInvocation* invocation_{};
        std::deque<InvocationFrame> frames_;
        size_t leased_frames_{};

    public:
        FrameLifecycleService() = default;
        FrameLifecycleService(const FrameLifecycleService&) = delete;
        FrameLifecycleService& operator=(const FrameLifecycleService&) = delete;
        FrameLifecycleSubscription_t Subscribe(FrameLifecycleObserverInterface* observer) override;
        void Unsubscribe(FrameLifecycleSubscription_t subscription) override;
        void Clear();
        void ReportFilter(int result);
        void BeginBody();
        void EndBody();

    private:
        InvocationFrame& AcquireFrame();
        FrameLifecycleObserverInterface* FindObserver(FrameLifecycleSubscription_t subscription) const;
    };

    class FrameLifecycleInvocation
    {
    private:
        FrameLifecycleService& service_;
        FrameLifecycleInvocation* previous_{};
        FrameLifecycleService::InvocationFrame& frame_;
        bool accepted_{};
        uint32_t body_depth_{};

    public:
        explicit FrameLifecycleInvocation(FrameLifecycleService& service);
        ~FrameLifecycleInvocation();
        FrameLifecycleInvocation(const FrameLifecycleInvocation&) = delete;
        FrameLifecycleInvocation& operator=(const FrameLifecycleInvocation&) = delete;
        void ReportFilter(int result);
        void BeginBody();
        void EndBody();

    private:
        void NotifyAccepted();
    };

    FrameLifecycleService& FrameLifecycle_GetDefaultService();
    FrameLifecycleInterface* FrameLifecycle_GetInterface();
    void FrameLifecycle_ClearObservers();

    template <class TCallable>
    decltype(auto) FrameLifecycle_InvokeHost(FrameLifecycleService& service, TCallable&& callback)
    {
        FrameLifecycleInvocation invocation(service);
        return std::forward<TCallable>(callback)();
    }
} // namespace nitroapi
