#pragma once

#include <memory>

namespace nitroapi
{
    class FrameLifecycleService;

    class HostAbortAdapter final
    {
    private:
        class Entry;
        FrameLifecycleService& service_;
        std::unique_ptr<Entry> entry_;

    public:
        explicit HostAbortAdapter(FrameLifecycleService& service);
        ~HostAbortAdapter();
        HostAbortAdapter(const HostAbortAdapter&) = delete;
        HostAbortAdapter& operator=(const HostAbortAdapter&) = delete;
        void Install(void* target);
        void Uninstall();
    };
} // namespace nitroapi
