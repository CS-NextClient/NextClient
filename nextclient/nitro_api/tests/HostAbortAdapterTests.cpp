#include <gtest/gtest.h>

#include <cstdarg>
#include <csetjmp>
#include <string>
#include <vector>

#include "FrameLifecycle.h"
#include "FrameAbortTestSupport.h"
#include "modules/engine/HostAbortAdapter.h"

namespace
{
    std::jmp_buf g_Abort;
    bool g_ShouldAbort{};
    const char* g_Format{};
    int g_Integer{};
    void* g_Pointer{};
    double g_Double{};

#ifdef _MSC_VER
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    void EndGame(const char* format, ...)
    {
        g_Format = format;

        va_list arguments;
        va_start(arguments, format);
        g_Integer = va_arg(arguments, int);
        g_Pointer = va_arg(arguments, void*);
        g_Double = va_arg(arguments, double);
        va_end(arguments);

        if (g_ShouldAbort)
        {
            frame_abort_test::Abort(g_Abort, 1);
        }
    }

    void InternalHost(nitroapi::FrameLifecycleService& service)
    {
        service.ReportFilter(1);

#ifdef _MSC_VER
        int aborted = frame_abort_test::Save(g_Abort);
#else
        int aborted = setjmp(g_Abort);
#endif

        if (!aborted)
        {
            EndGame("%d %p %.3f", -127, &service, 3.125);
        }
    }

    class AbortObserver final : public nitroapi::FrameLifecycleObserverInterface
    {
    public:
        std::vector<std::string> events;

        void OnHostEntry() override
        {
            events.emplace_back("entry");
        }

        void OnFrameAccepted() override
        {
            events.emplace_back("accepted");
        }

        void OnHostExit(bool completed) override
        {
            events.emplace_back(completed ? "complete" : "abort");
        }
    };
} // namespace

TEST(HostAbortAdapterTest, RepeatedInternalJumpsPreserveVariadicArgumentsAndNextFrame)
{
    nitroapi::FrameLifecycleService service;
    AbortObserver observer;
    service.Subscribe(&observer);

    nitroapi::HostAbortAdapter adapter(service);
    adapter.Install(reinterpret_cast<void*>(&EndGame));
    g_ShouldAbort = true;

    for (int index = 0; index < 24; ++index)
    {
        observer.events.clear();
        nitroapi::FrameLifecycle_InvokeHost(service, [&] {
            service.BeginBody();
            InternalHost(service);
            service.EndBody();
        });

        EXPECT_EQ(observer.events, (std::vector<std::string>{"entry", "accepted", "abort"}));
        EXPECT_STREQ(g_Format, "%d %p %.3f");
        EXPECT_EQ(g_Integer, -127);
        EXPECT_EQ(g_Pointer, &service);
        EXPECT_DOUBLE_EQ(g_Double, 3.125);
    }

    observer.events.clear();
    nitroapi::FrameLifecycle_InvokeHost(service, [&] { service.ReportFilter(1); });
    EXPECT_EQ(observer.events, (std::vector<std::string>{"entry", "accepted", "complete"}));

    service.Clear();
}

TEST(HostAbortAdapterTest, UninstallAndDestructionRestoreOriginalAndAllowReinstall)
{
    nitroapi::FrameLifecycleService service;
    AbortObserver observer;
    service.Subscribe(&observer);
    g_ShouldAbort = false;
    {
        nitroapi::HostAbortAdapter adapter(service);

        for (int index = 0; index < 3; ++index)
        {
            adapter.Install(reinterpret_cast<void*>(&EndGame));
            observer.events.clear();
            nitroapi::FrameLifecycle_InvokeHost(service, [&] { InternalHost(service); });
            EXPECT_EQ(observer.events.back(), "abort");

            adapter.Uninstall();
            adapter.Uninstall();
            observer.events.clear();
            nitroapi::FrameLifecycle_InvokeHost(service, [&] { InternalHost(service); });
            EXPECT_EQ(observer.events.back(), "complete");
        }

        adapter.Install(reinterpret_cast<void*>(&EndGame));
    }

    observer.events.clear();
    nitroapi::FrameLifecycle_InvokeHost(service, [&] { InternalHost(service); });
    EXPECT_EQ(observer.events.back(), "complete");
    EXPECT_EQ(g_Pointer, &service);

    service.Clear();
}

TEST(HostAbortAdapterTest, ReportsOnlyBoundOwnerAndCurrentNestedInvocation)
{
    nitroapi::FrameLifecycleService first;
    nitroapi::FrameLifecycleService second;
    AbortObserver a;
    AbortObserver b;
    first.Subscribe(&a);
    second.Subscribe(&b);
    g_ShouldAbort = false;

    nitroapi::HostAbortAdapter adapter(first);
    adapter.Install(reinterpret_cast<void*>(&EndGame));
    InternalHost(first);

    nitroapi::FrameLifecycle_InvokeHost(first, [&] {
        nitroapi::FrameLifecycle_InvokeHost(second, [&] { InternalHost(second); });
    });

    EXPECT_EQ(a.events.back(), "abort");
    EXPECT_EQ(b.events.back(), "complete");

    a.events.clear();
    nitroapi::FrameLifecycle_InvokeHost(first, [&] {
        nitroapi::FrameLifecycle_InvokeHost(first, [&] { InternalHost(first); });
    });

    EXPECT_EQ(a.events, (std::vector<std::string>{"entry", "entry", "accepted", "abort", "complete"}));

    a.events.clear();
    nitroapi::FrameLifecycle_InvokeHost(first, [] {});
    EXPECT_EQ(a.events.back(), "complete");

    first.Clear();
    second.Clear();
}
