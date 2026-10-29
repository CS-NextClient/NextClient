#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <csetjmp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <easylogging++.h>

#include "FrameLifecycle.h"
#include "hook/GenericHookStatic.h"
#include "chain/HandlersChain.h"

INITIALIZE_EASYLOGGINGPP

#ifdef _WIN32
namespace nitroapi
{
    LONG WINAPI GlobalExceptionHandlerWin32(EXCEPTION_POINTERS*)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
} // namespace nitroapi
#endif

namespace
{
#ifdef _MSC_VER
    // Model the engine's nonlocal frame abort without C++ stack unwinding.
    __declspec(naked) int SaveFrameAbort(std::jmp_buf)
    {
        __asm {
            mov edx, [esp + 4]
            mov [edx], ebp
            mov [edx + 4], ebx
            mov [edx + 8], edi
            mov [edx + 12], esi
            mov [edx + 16], esp
            mov eax, [esp]
            mov [edx + 20], eax
            mov eax, fs:[0]
            mov [edx + 24], eax
            xor eax, eax
            ret
        }
    }

    __declspec(naked) void AbortFrame(std::jmp_buf, int)
    {
        __asm {
            mov edx, [esp + 4]
            mov ebp, [edx]
            mov ebx, [edx + 4]
            mov edi, [edx + 8]
            mov esi, [edx + 12]
            mov eax, [edx + 24]
            mov fs:[0], eax
            mov eax, [esp + 8]
            mov esp, [edx + 16]
            add esp, 4
            jmp dword ptr [edx + 20]
        }
    }
#endif

    int g_FilterCalls{};
    float g_FilterDelta{};
    int g_FilterResult = 7;

#ifdef _MSC_VER
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    int FilterTime(float delta)
    {
        ++g_FilterCalls;
        g_FilterDelta = delta;
        return g_FilterResult;
    }

    std::jmp_buf g_ClientFrameAbort;
    bool g_AbortClientFrame{};

#ifdef _MSC_VER
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    void InternalHost(float)
    {
        nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
        if (g_AbortClientFrame)
        {
#ifdef _MSC_VER
            AbortFrame(g_ClientFrameAbort, 1);
#else
            std::longjmp(g_ClientFrameAbort, 1);
#endif
        }
    }

#ifdef _MSC_VER
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    int ClientHost(float delta, int state, int* state_info)
    {
        *state_info = 0;
#ifdef _MSC_VER
        int aborted = SaveFrameAbort(g_ClientFrameAbort);
#else
        int aborted = setjmp(g_ClientFrameAbort);
#endif
        if (!aborted)
        {
            InternalHost(delta);
            *state_info = 42;
        }
        return state + aborted;
    }

    class Observer : public nitroapi::FrameLifecycleObserverInterface
    {
    private:
        std::vector<std::string>& events_;

    public:
        explicit Observer(std::vector<std::string>& events) :
            events_(events)
        {}
        void OnHostEntry() override
        {
            events_.emplace_back("host.enter");
        }
        void OnFrameAccepted() override
        {
            events_.emplace_back("filter.accept");
        }
        void OnHostExit(bool completed) override
        {
            events_.emplace_back(completed ? "host.exit" : "host.abort");
        }
    };

    class FakeHostHook : public nitroapi::GenericHookInterface<void, float>
    {
    public:
        nitroapi::FrameLifecycleService& service;

        explicit FakeHostHook(nitroapi::FrameLifecycleService& lifecycle) :
            service(lifecycle)
        {}

        std::function<void(float)> callback;
        std::function<void(float)> original;
        void SetCallbackAndEnableHook(std::function<void(float)>&& value) override
        {
            callback = std::move(value);
        }
        void UnsetCallbackAndDisableHook() override
        {
            callback = {};
        }
        uint32_t GetTrampolineAddress() const override
        {
            return 0;
        }
        uint32_t GetOriginalFunctionAddress() const override
        {
            return 0;
        }
        bool IsHookActive() const override
        {
            return bool(callback);
        }
        void InvokeOriginal(float delta) override
        {
            original(delta);
        }
        void Invoke(float delta) override
        {
            nitroapi::FrameLifecycle_InvokeHost(service, [&] { callback(delta); });
        }
    };

    class Subscriber : public nitroapi::SubscriberInterface<void, float>
    {
    private:
        std::vector<std::string>& events_;
        const char* name_;

    public:
        Subscriber(std::vector<std::string>& events, const char* name) :
            events_(events),
            name_(name)
        {}
        void OnEvent(float, nitroapi::VoidResult) override
        {
            events_.emplace_back(name_);
        }
    };

    class FrameLifecycleTest : public testing::Test
    {
    protected:
        std::vector<std::string> events;
        Observer observer{events};
        nitroapi::FrameLifecycleSubscription_t subscription{};

        void SetUp() override
        {
            nitroapi::FrameLifecycle_ClearObservers();
            subscription = nitroapi::FrameLifecycle_GetInterface()->Subscribe(&observer);
        }
        void TearDown() override
        {
            nitroapi::FrameLifecycle_ClearObservers();
        }
    };
} // namespace

TEST_F(FrameLifecycleTest, IndependentSubscriptionsReceiveEveryBoundary)
{
    std::vector<std::string> second_events;
    Observer second(second_events);
    nitroapi::FrameLifecycleSubscription_t second_subscription = nitroapi::FrameLifecycle_GetInterface()->Subscribe(&second);
    ASSERT_NE(subscription, 0u);
    ASSERT_NE(second_subscription, 0u);
    ASSERT_NE(subscription, second_subscription);

    nitroapi::FrameLifecycle_InvokeHost(nitroapi::FrameLifecycle_GetDefaultService(), [&] {
        nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
    });
    EXPECT_EQ(events, (std::vector<std::string>{"host.enter", "filter.accept", "host.exit"}));
    EXPECT_EQ(second_events, (std::vector<std::string>{"host.enter", "filter.accept", "host.exit"}));
    nitroapi::FrameLifecycle_GetInterface()->Unsubscribe(second_subscription);
}

TEST_F(FrameLifecycleTest, ExitRunsAfterEverySubscriber)
{
    FakeHostHook hook(nitroapi::FrameLifecycle_GetDefaultService());
    nitroapi::HandlersChain<void, float> chain;
    Subscriber a(events, "subscriber.a");
    Subscriber b(events, "subscriber.b");
    chain.Subscribe(&a);
    chain.Subscribe(&b);
    hook.original = [&](float) {
        nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
        events.emplace_back("body");
    };
    chain.AttachHook(&hook);
    hook.Invoke(0.02f);
    EXPECT_EQ(events, (std::vector<std::string>{"host.enter", "filter.accept", "body", "subscriber.b", "subscriber.a", "host.exit"}));
    chain.DetachHook();
}

TEST_F(FrameLifecycleTest, RejectedFilterHasNoAcceptedFrame)
{
    nitroapi::FrameLifecycle_InvokeHost(nitroapi::FrameLifecycle_GetDefaultService(), [&] {
        nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(0);
    });
    EXPECT_EQ(events, (std::vector<std::string>{"host.enter", "host.exit"}));
    nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
    EXPECT_EQ(events.size(), 2u);
}

TEST_F(FrameLifecycleTest, ObserverDetachCannotCallDestroyedTarget)
{
    nitroapi::FrameLifecycle_InvokeHost(nitroapi::FrameLifecycle_GetDefaultService(), [&] {
        nitroapi::FrameLifecycle_ClearObservers();
        nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
    });
    EXPECT_EQ(events, (std::vector<std::string>{"host.enter"}));
}

TEST_F(FrameLifecycleTest, NestedInvocationPreservesOuterState)
{
    nitroapi::FrameLifecycle_InvokeHost(nitroapi::FrameLifecycle_GetDefaultService(), [&] {
        nitroapi::FrameLifecycle_InvokeHost(nitroapi::FrameLifecycle_GetDefaultService(), [&] {
            nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
        });
        nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
        nitroapi::FrameLifecycle_GetDefaultService().ReportFilter(1);
    });
    EXPECT_EQ(events, (std::vector<std::string>{"host.enter", "host.enter", "filter.accept", "host.exit", "filter.accept", "host.exit"}));
}

TEST_F(FrameLifecycleTest, ForwardingPreservesResultAndArguments)
{
    g_FilterCalls = 0;
    g_FilterDelta = 0;
    g_FilterResult = 7;
    nitroapi::GenericHookStatic<nitroapi::StaticHookId::Host_FilterTime, nitroapi::CallingConventions::Cdecl, int, float> hook(
        uint32_t(reinterpret_cast<uintptr_t>(&FilterTime))
    );
    hook.SetCallbackAndEnableHook([&](float delta) {
        events.emplace_back("filter.callback");
        return hook.InvokeOriginal(delta);
    });
    ASSERT_TRUE(hook.IsHookActive());

    int result = 0;
    nitroapi::FrameLifecycle_InvokeHost(nitroapi::FrameLifecycle_GetDefaultService(), [&] { result = hook.Invoke(0.125f); });
    hook.UnsetCallbackAndDisableHook();

    EXPECT_EQ(g_FilterCalls, 1);
    EXPECT_FLOAT_EQ(g_FilterDelta, 0.125f);
    EXPECT_EQ(result, 7);
    EXPECT_EQ(events, (std::vector<std::string>{"host.enter", "filter.callback", "filter.accept", "host.exit"}));
}

TEST_F(FrameLifecycleTest, ClientFrameAbortDoesNotLoseSubsequentBoundaries)
{
    nitroapi::GenericHookStatic<nitroapi::StaticHookId::Host_FrameInternal, nitroapi::CallingConventions::Cdecl, void, float> inner(
        uint32_t(reinterpret_cast<uintptr_t>(&InternalHost))
    );
    nitroapi::GenericHookStatic<nitroapi::StaticHookId::Host_Frame, nitroapi::CallingConventions::Cdecl, int, float, int, int*> outer(
        uint32_t(reinterpret_cast<uintptr_t>(&ClientHost))
    );
    inner.SetCallbackAndEnableHook([&](float delta) { inner.InvokeOriginal(delta); });
    outer.SetCallbackAndEnableHook([&](float delta, int state, int* state_info) { return outer.InvokeOriginal(delta, state, state_info); });
    ASSERT_TRUE(inner.IsHookActive());
    ASSERT_TRUE(outer.IsHookActive());

    int state_info = -1;
    g_AbortClientFrame = true;
    EXPECT_EQ(outer.Invoke(0.01f, 5, &state_info), 6);
    EXPECT_EQ(state_info, 0);
    g_AbortClientFrame = false;
    EXPECT_EQ(outer.Invoke(0.02f, 7, &state_info), 7);
    EXPECT_EQ(state_info, 42);
    outer.UnsetCallbackAndDisableHook();
    inner.UnsetCallbackAndDisableHook();

    EXPECT_EQ(events, (std::vector<std::string>{"host.enter", "filter.accept", "host.abort", "host.enter", "filter.accept", "host.exit"}));
}

namespace
{
    enum class NotificationPhase
    {
        Entry,
        Accepted,
        Exit
    };

    class NamedObserver : public nitroapi::FrameLifecycleObserverInterface
    {
    private:
        std::vector<std::string>& events_;
        std::string name_;

    public:
        std::function<void(NotificationPhase)> action;

        NamedObserver(std::vector<std::string>& events, const char* name) :
            events_(events),
            name_(name)
        {}

        void OnHostEntry() override
        {
            Notify(NotificationPhase::Entry, ".enter");
        }

        void OnFrameAccepted() override
        {
            Notify(NotificationPhase::Accepted, ".accept");
        }

        void OnHostExit(bool completed) override
        {
            Notify(NotificationPhase::Exit, completed ? ".exit" : ".abort");
        }

    private:
        void Notify(NotificationPhase phase, const char* suffix)
        {
            events_.emplace_back(name_ + suffix);
            std::function<void(NotificationPhase)> callback = action;

            if (callback)
            {
                callback(phase);
            }
        }
    };

    class FrameSubscriptionsTest : public testing::Test
    {
    protected:
        nitroapi::FrameLifecycleService owner;
        nitroapi::FrameLifecycleService* service = &owner;
        std::vector<std::string> events;

        void SetUp() override
        {
            service->Clear();
        }

        void TearDown() override
        {
            service->Clear();
        }

        void RunHost()
        {
            nitroapi::FrameLifecycle_InvokeHost(*service, [&] {
                service->ReportFilter(1);
                service->ReportFilter(1);
                events.emplace_back("body");
            });
        }
    };

    class FrameSubscriptionMutationTest : public FrameSubscriptionsTest, public testing::WithParamInterface<NotificationPhase>
    {};
} // namespace

TEST_F(FrameSubscriptionsTest, OrderedBoundariesSurroundEveryOrdinarySubscriber)
{
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver c(events, "c");
    service->Subscribe(&a);
    service->Subscribe(&b);
    service->Subscribe(&c);

    FakeHostHook hook(*service);
    nitroapi::HandlersChain<void, float> chain;
    Subscriber first(events, "subscriber.first");
    Subscriber second(events, "subscriber.second");
    chain.Subscribe(&first);
    chain.Subscribe(&second);
    hook.original = [&](float) {
        service->ReportFilter(1);
        service->ReportFilter(1);
        events.emplace_back("body");
    };
    chain.AttachHook(&hook);
    hook.Invoke(0.02f);
    chain.DetachHook();

    EXPECT_EQ(
        events,
        (std::vector<std::string>{
            "a.enter",
            "b.enter",
            "c.enter",
            "a.accept",
            "b.accept",
            "c.accept",
            "body",
            "subscriber.second",
            "subscriber.first",
            "c.exit",
            "b.exit",
            "a.exit"
        })
    );

    service->Clear();
}

TEST_F(FrameSubscriptionsTest, RejectedFilterPreservesEveryRegistration)
{
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    service->Subscribe(&a);
    service->Subscribe(&b);
    nitroapi::FrameLifecycle_InvokeHost(*service, [&] { service->ReportFilter(0); });
    EXPECT_EQ(events, (std::vector<std::string>{"a.enter", "b.enter", "b.exit", "a.exit"}));

    service->Clear();
}

TEST_F(FrameSubscriptionsTest, RemovalIsIndependentAndIdempotent)
{
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver c(events, "c");
    nitroapi::FrameLifecycleSubscription_t first = service->Subscribe(&a);
    nitroapi::FrameLifecycleSubscription_t removed = service->Subscribe(&b);
    nitroapi::FrameLifecycleSubscription_t third = service->Subscribe(&c);
    EXPECT_NE(first, removed);
    EXPECT_NE(removed, third);
    EXPECT_EQ(service->Subscribe(nullptr), 0u);
    service->Unsubscribe(removed);
    service->Unsubscribe(removed);
    service->Unsubscribe(0);
    RunHost();
    EXPECT_EQ(events, (std::vector<std::string>{"a.enter", "c.enter", "a.accept", "c.accept", "body", "c.exit", "a.exit"}));

    service->Clear();
}

TEST_F(FrameSubscriptionsTest, DuplicatePointersHaveIndependentIds)
{
    NamedObserver a(events, "a");
    nitroapi::FrameLifecycleSubscription_t first = service->Subscribe(&a);
    nitroapi::FrameLifecycleSubscription_t second = service->Subscribe(&a);
    ASSERT_NE(first, second);
    RunHost();
    EXPECT_EQ(events, (std::vector<std::string>{"a.enter", "a.enter", "a.accept", "a.accept", "body", "a.exit", "a.exit"}));

    events.clear();
    service->Unsubscribe(first);
    RunHost();
    EXPECT_EQ(events, (std::vector<std::string>{"a.enter", "a.accept", "body", "a.exit"}));

    service->Clear();
}

TEST_F(FrameSubscriptionsTest, ClearDoesNotReuseIdsOrReviveStaleTokens)
{
    NamedObserver a(events, "a");
    nitroapi::FrameLifecycleSubscription_t stale = service->Subscribe(&a);
    service->Clear();
    nitroapi::FrameLifecycleSubscription_t current = service->Subscribe(&a);
    ASSERT_GT(current, stale);
    service->Unsubscribe(stale);
    RunHost();
    EXPECT_EQ(events, (std::vector<std::string>{"a.enter", "a.accept", "body", "a.exit"}));

    service->Clear();
}

TEST_F(FrameSubscriptionsTest, AcceptanceDuringEntryWaitsForEachRecipientToEnter)
{
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver c(events, "c");
    a.action = [&](NotificationPhase) { service->ReportFilter(1); };
    service->Subscribe(&a);
    service->Subscribe(&b);
    service->Subscribe(&c);
    RunHost();
    EXPECT_EQ(
        events,
        (std::vector<std::string>{
            "a.enter", "a.accept", "b.enter", "b.accept", "c.enter", "c.accept", "body", "c.exit", "b.exit", "a.exit"
        })
    );

    service->Clear();
}

TEST_F(FrameSubscriptionsTest, ReregisteredPointerParticipatesOnlyInNewNestedInvocation)
{
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver c(events, "c");
    service->Subscribe(&a);
    nitroapi::FrameLifecycleSubscription_t original = service->Subscribe(&b);
    service->Subscribe(&c);
    nitroapi::FrameLifecycleSubscription_t replacement = 0;
    bool mutated = false;
    a.action = [&](NotificationPhase phase) {
        if (phase == NotificationPhase::Entry && !mutated)
        {
            mutated = true;
            service->Unsubscribe(original);
            replacement = service->Subscribe(&b);
            RunHost();
        }
    };
    RunHost();
    EXPECT_GT(replacement, original);
    EXPECT_EQ(
        events,
        (std::vector<std::string>{
            "a.enter",
            "a.enter",
            "c.enter",
            "b.enter",
            "a.accept",
            "c.accept",
            "b.accept",
            "body",
            "b.exit",
            "c.exit",
            "a.exit",
            "c.enter",
            "a.accept",
            "c.accept",
            "body",
            "c.exit",
            "a.exit"
        })
    );

    service->Clear();
}

TEST_P(FrameSubscriptionMutationTest, SelfRemovalCanDestroyObserver)
{
    std::unique_ptr<NamedObserver> a = std::make_unique<NamedObserver>(events, "a");
    NamedObserver b(events, "b");
    NamedObserver c(events, "c");
    nitroapi::FrameLifecycleSubscription_t removed = service->Subscribe(a.get());
    service->Subscribe(&b);
    service->Subscribe(&c);
    a->action = [&](NotificationPhase phase) {
        if (phase == GetParam())
        {
            service->Unsubscribe(removed);
            a.reset();
        }
    };
    RunHost();
    std::vector<std::string> expected;

    if (GetParam() == NotificationPhase::Entry)
    {
        expected = {"a.enter", "b.enter", "c.enter", "b.accept", "c.accept", "body", "c.exit", "b.exit"};
    }
    else if (GetParam() == NotificationPhase::Accepted)
    {
        expected = {"a.enter", "b.enter", "c.enter", "a.accept", "b.accept", "c.accept", "body", "c.exit", "b.exit"};
    }
    else
    {
        expected = {"a.enter", "b.enter", "c.enter", "a.accept", "b.accept", "c.accept", "body", "c.exit", "b.exit", "a.exit"};
    }

    EXPECT_EQ(events, expected);
    EXPECT_FALSE(a);

    service->Clear();
}

TEST_P(FrameSubscriptionMutationTest, RemovalCanDestroyLaterRecipient)
{
    NamedObserver a(events, "a");
    std::unique_ptr<NamedObserver> b = std::make_unique<NamedObserver>(events, "b");
    NamedObserver c(events, "c");
    service->Subscribe(&a);
    nitroapi::FrameLifecycleSubscription_t removed = service->Subscribe(b.get());
    service->Subscribe(&c);
    std::function<void(NotificationPhase)> remove = [&](NotificationPhase phase) {
        if (phase == GetParam())
        {
            service->Unsubscribe(removed);
            b.reset();
        }
    };

    if (GetParam() == NotificationPhase::Exit)
    {
        c.action = remove;
    }
    else
    {
        a.action = remove;
    }

    RunHost();
    std::vector<std::string> expected;

    if (GetParam() == NotificationPhase::Entry)
    {
        expected = {"a.enter", "c.enter", "a.accept", "c.accept", "body", "c.exit", "a.exit"};
    }
    else if (GetParam() == NotificationPhase::Accepted)
    {
        expected = {"a.enter", "b.enter", "c.enter", "a.accept", "c.accept", "body", "c.exit", "a.exit"};
    }
    else
    {
        expected = {"a.enter", "b.enter", "c.enter", "a.accept", "b.accept", "c.accept", "body", "c.exit", "a.exit"};
    }

    EXPECT_EQ(events, expected);
    EXPECT_FALSE(b);

    service->Clear();
}

TEST_P(FrameSubscriptionMutationTest, ClearInvalidatesRemainingCallbacks)
{
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver c(events, "c");
    service->Subscribe(&a);
    service->Subscribe(&b);
    service->Subscribe(&c);
    std::function<void(NotificationPhase)> clear = [&](NotificationPhase phase) {
        if (phase == GetParam())
        {
            service->Clear();
        }
    };

    if (GetParam() == NotificationPhase::Exit)
    {
        c.action = clear;
    }
    else
    {
        a.action = clear;
    }

    RunHost();
    std::vector<std::string> expected;

    if (GetParam() == NotificationPhase::Entry)
    {
        expected = {"a.enter", "body"};
    }
    else if (GetParam() == NotificationPhase::Accepted)
    {
        expected = {"a.enter", "b.enter", "c.enter", "a.accept", "body"};
    }
    else
    {
        expected = {"a.enter", "b.enter", "c.enter", "a.accept", "b.accept", "c.accept", "body", "c.exit"};
    }

    EXPECT_EQ(events, expected);

    service->Clear();
}

TEST_P(FrameSubscriptionMutationTest, AddedRegistrationWaitsForTheNextInvocation)
{
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver added(events, "added");
    service->Subscribe(&a);
    service->Subscribe(&b);
    nitroapi::FrameLifecycleSubscription_t added_subscription = 0;
    a.action = [&](NotificationPhase phase) {
        if (phase == GetParam() && !added_subscription)
        {
            added_subscription = service->Subscribe(&added);
        }
    };
    RunHost();
    EXPECT_EQ(events, (std::vector<std::string>{"a.enter", "b.enter", "a.accept", "b.accept", "body", "b.exit", "a.exit"}));
    ASSERT_NE(added_subscription, 0u);

    events.clear();
    RunHost();
    EXPECT_EQ(
        events,
        (std::vector<std::string>{
            "a.enter", "b.enter", "added.enter", "a.accept", "b.accept", "added.accept", "body", "added.exit", "b.exit", "a.exit"
        })
    );

    service->Clear();
}

INSTANTIATE_TEST_SUITE_P(
    EveryBoundary,
    FrameSubscriptionMutationTest,
    testing::Values(NotificationPhase::Entry, NotificationPhase::Accepted, NotificationPhase::Exit)
);

TEST(FrameLifecycleOwnerTest, IndependentOwnersPreserveNestedAbaInvocation)
{
    nitroapi::FrameLifecycleService first;
    nitroapi::FrameLifecycleService second;
    std::vector<std::string> events;
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    nitroapi::FrameLifecycleSubscription_t first_id = first.Subscribe(&a);
    nitroapi::FrameLifecycleSubscription_t second_id = second.Subscribe(&b);
    EXPECT_EQ(first_id, second_id);

    nitroapi::FrameLifecycle_InvokeHost(first, [&] {
        first.BeginBody();
        nitroapi::FrameLifecycle_InvokeHost(second, [&] {
            nitroapi::FrameLifecycle_InvokeHost(first, [&] { first.ReportFilter(1); });
            second.ReportFilter(1);
            first.ReportFilter(1);
        });
    });

    EXPECT_EQ(
        events,
        (std::vector<std::string>{"a.enter", "b.enter", "a.enter", "a.accept", "a.exit", "b.accept", "a.accept", "b.exit", "a.abort"})
    );
    first.Clear();
    events.clear();
    nitroapi::FrameLifecycle_InvokeHost(second, [&] { second.ReportFilter(1); });
    EXPECT_EQ(events, (std::vector<std::string>{"b.enter", "b.accept", "b.exit"}));
    second.Clear();
}

TEST(FrameLifecycleOwnerTest, ExitReentryKeepsOutgoingRecipientsAndCompletion)
{
    nitroapi::FrameLifecycleService service;
    std::vector<std::string> events;
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver c(events, "c");
    NamedObserver added(events, "added");
    nitroapi::FrameLifecycleSubscription_t removed = service.Subscribe(&a);
    service.Subscribe(&b);
    service.Subscribe(&c);
    bool reentered = false;
    c.action = [&](NotificationPhase phase) {
        if (phase == NotificationPhase::Exit && !reentered)
        {
            reentered = true;
            service.Unsubscribe(removed);
            service.Subscribe(&added);
            nitroapi::FrameLifecycle_InvokeHost(service, [&] { service.ReportFilter(1); });
        }
    };

    nitroapi::FrameLifecycle_InvokeHost(service, [&] {
        service.ReportFilter(1);
        service.BeginBody();
    });

    EXPECT_EQ(
        events,
        (std::vector<std::string>{
            "a.enter",
            "b.enter",
            "c.enter",
            "a.accept",
            "b.accept",
            "c.accept",
            "c.abort",
            "b.enter",
            "c.enter",
            "added.enter",
            "b.accept",
            "c.accept",
            "added.accept",
            "added.exit",
            "c.exit",
            "b.exit",
            "b.abort"
        })
    );

    service.Clear();
}

namespace nitroapi
{
    class FrameLifecycleStorageTestAccess
    {
    public:
        struct Slot
        {
            const void* frame{};
            const void* recipients{};
            size_t capacity{};

            bool operator==(const Slot&) const = default;
        };

        static std::vector<Slot> Capture(const FrameLifecycleService& service)
        {
            std::vector<Slot> slots;

            for (const FrameLifecycleService::InvocationFrame& frame : service.frames_)
            {
                slots.push_back({&frame, frame.recipients.data(), frame.recipients.capacity()});
            }

            return slots;
        }

        static size_t LeasedFrames(const FrameLifecycleService& service)
        {
            return service.leased_frames_;
        }
    };
} // namespace nitroapi

namespace
{
    class CountingObserver : public nitroapi::FrameLifecycleObserverInterface
    {
    public:
        size_t entries{};
        size_t accepted{};
        size_t exits{};
        size_t aborted{};

        void OnHostEntry() override
        {
            ++entries;
        }

        void OnFrameAccepted() override
        {
            ++accepted;
        }

        void OnHostExit(bool completed) override
        {
            ++exits;
            aborted += !completed;
        }
    };

    void RunNestedHosts(nitroapi::FrameLifecycleService& service, size_t depth)
    {
        nitroapi::FrameLifecycle_InvokeHost(service, [&] {
            service.ReportFilter(1);
            service.BeginBody();

            if (depth > 1)
            {
                RunNestedHosts(service, depth - 1);
            }

            service.EndBody();
        });
    }
} // namespace

TEST(FrameLifecycleOwnerTest, WarmedRecipientStorageRetainsEverySlotAndCapacity)
{
    nitroapi::FrameLifecycleService service;
    std::array<CountingObserver, 129> observers;
    size_t registered = 0;

    for (size_t count : {size_t(0), size_t(1), observers.size()})
    {
        for (; registered < count; ++registered)
        {
            service.Subscribe(&observers[registered]);
        }

        for (size_t depth : {size_t(1), size_t(12), size_t(36)})
        {
            RunNestedHosts(service, depth);
            std::vector<nitroapi::FrameLifecycleStorageTestAccess::Slot> warmed =
                nitroapi::FrameLifecycleStorageTestAccess::Capture(service);
            size_t previous_entries = observers[0].entries;

            // Callbacks only update scalar counters inside this storage-reuse interval.
            for (size_t repeat = 0; repeat < 8; ++repeat)
            {
                RunNestedHosts(service, depth);
            }

            EXPECT_EQ(nitroapi::FrameLifecycleStorageTestAccess::Capture(service), warmed);
            EXPECT_EQ(nitroapi::FrameLifecycleStorageTestAccess::LeasedFrames(service), 0u);
            EXPECT_EQ(observers[0].entries - previous_entries, count ? depth * 8 : 0u);
        }
    }

    for (const CountingObserver& observer : observers)
    {
        EXPECT_GT(observer.entries, 0u);
        EXPECT_EQ(observer.entries, observer.accepted);
        EXPECT_EQ(observer.entries, observer.exits);
        EXPECT_EQ(observer.aborted, 0u);
    }

    service.Clear();
}

TEST(FrameLifecycleOwnerTest, ExitClearRetainsLeasedFramesAndSkipsStaleIds)
{
    nitroapi::FrameLifecycleService service;
    std::vector<std::string> events;
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    NamedObserver replacement(events, "replacement");
    nitroapi::FrameLifecycleSubscription_t stale = service.Subscribe(&a);
    service.Subscribe(&b);
    nitroapi::FrameLifecycleSubscription_t fresh = 0;
    b.action = [&](NotificationPhase phase) {
        if (phase == NotificationPhase::Exit)
        {
            service.Clear();
            fresh = service.Subscribe(&replacement);
            service.Unsubscribe(stale);
            RunNestedHosts(service, 18);
        }
    };

    RunNestedHosts(service, 1);
    EXPECT_GT(fresh, stale);
    ASSERT_EQ(events.size(), 5u + 18u * 3u);
    EXPECT_EQ(events[4], "b.exit");
    EXPECT_EQ(events.back(), "replacement.exit");
    EXPECT_EQ(nitroapi::FrameLifecycleStorageTestAccess::LeasedFrames(service), 0u);
    EXPECT_EQ(std::count(events.begin(), events.end(), "a.exit"), 0);

    service.Clear();
}

TEST(FrameLifecycleOwnerTest, ObserverCallbacksCanNestAnotherOwnerAndReturnToTheFirst)
{
    nitroapi::FrameLifecycleService first;
    nitroapi::FrameLifecycleService second;
    std::vector<std::string> events;
    NamedObserver a(events, "a");
    NamedObserver b(events, "b");
    first.Subscribe(&a);
    second.Subscribe(&b);
    bool nested = false;
    a.action = [&](NotificationPhase phase) {
        if (phase == NotificationPhase::Entry && !nested)
        {
            nested = true;
            nitroapi::FrameLifecycle_InvokeHost(second, [&] { second.ReportFilter(1); });
        }
    };
    b.action = [&](NotificationPhase phase) {
        if (phase == NotificationPhase::Accepted)
        {
            nitroapi::FrameLifecycle_InvokeHost(first, [&] { first.ReportFilter(1); });
        }
    };

    nitroapi::FrameLifecycle_InvokeHost(first, [&] { first.ReportFilter(1); });
    EXPECT_EQ(
        events,
        (std::vector<std::string>{"a.enter", "b.enter", "b.accept", "a.enter", "a.accept", "a.exit", "b.exit", "a.accept", "a.exit"})
    );

    first.Clear();
    second.Clear();
}
