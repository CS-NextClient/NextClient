#include <gtest/gtest.h>

#include <csetjmp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <goldsrc/engine/user_msg.h>

#include "FrameAbortTestSupport.h"
#include "FrameLifecycle.h"
#include "modules/engine/EngineModule.h"

namespace
{
    enum class AbortMode
    {
        None,
        Internal,
        Outer
    };

    struct EngineFrame
    {
        std::jmp_buf outer_abort;
        std::jmp_buf internal_abort;
        AbortMode abort_mode{};
        int filter_result = 7;
        int filter_calls{};
        int body_calls{};
        float filter_delta{};
    };

    EngineFrame* g_Frame{};

    // Native detours require the ABI instead of inferred register preservation at call sites.
#ifdef _MSC_VER
    __declspec(noinline)
#elif defined(__GNUC__) && !defined(__clang__)
    __attribute__((noipa))
#else
    __attribute__((noinline))
#endif
    int FilterTime(float delta)
    {
        ++g_Frame->filter_calls;
        g_Frame->filter_delta = delta;
        return g_Frame->filter_result;
    }

#ifdef _MSC_VER
    __declspec(noinline)
#elif defined(__GNUC__) && !defined(__clang__)
    __attribute__((noipa))
#else
    __attribute__((noinline))
#endif
    void EndGame(const char*, ...)
    {
        frame_abort_test::Abort(g_Frame->abort_mode == AbortMode::Internal ? g_Frame->internal_abort : g_Frame->outer_abort, 1);
    }

#ifdef _MSC_VER
    __declspec(noinline)
#elif defined(__GNUC__) && !defined(__clang__)
    __attribute__((noipa))
#else
    __attribute__((noinline))
#endif
    void InternalHost(float delta)
    {
        ++g_Frame->body_calls;
        if (!FilterTime(delta))
        {
            return;
        }

#ifdef _MSC_VER
        int aborted = frame_abort_test::Save(g_Frame->internal_abort);
#else
        int aborted = setjmp(g_Frame->internal_abort);
#endif

        if (!aborted && g_Frame->abort_mode != AbortMode::None)
        {
            EndGame("frame abort %d", 17);
        }
    }

#ifdef _MSC_VER
    __declspec(noinline)
#elif defined(__GNUC__) && !defined(__clang__)
    __attribute__((noipa))
#else
    __attribute__((noinline))
#endif
    int Host(float delta, int state, int* state_info)
    {
        *state_info = 0;

#ifdef _MSC_VER
        int aborted = frame_abort_test::Save(g_Frame->outer_abort);
#else
        int aborted = setjmp(g_Frame->outer_abort);
#endif

        if (!aborted)
        {
            InternalHost(delta);
            *state_info = 42;
        }

        return state + aborted;
    }

    goldsrc::EngineMsg g_Messages[] = {
        {0, const_cast<char*>("svc_nop"), {}},
        {0, const_cast<char*>("svc_disconnect"), {}},
        {0, const_cast<char*>("svc_event"), {}},
        {0, const_cast<char*>("svc_version"), {}},
        {0, const_cast<char*>("svc_setview"), {}},
        {0, const_cast<char*>("svc_sound"), {}},
        {0, const_cast<char*>("svc_time"), {}},
        {0, const_cast<char*>("svc_print"), {}},
        {0, const_cast<char*>("svc_stufftext"), {}},
        {0, const_cast<char*>("svc_resourcelist"), {}},
        {0, const_cast<char*>("svc_director"), {}},
        {0, const_cast<char*>("svc_signonnum"), {}},
        {0, const_cast<char*>("svc_setangle"), {}},
        {0, const_cast<char*>("svc_resourcelocation"), {}},
        {0, const_cast<char*>("svc_newusermsg"), {}},
        {0, const_cast<char*>("svc_sendcvarvalue"), {}},
        {0, const_cast<char*>("svc_updateuserinfo"), {}},
        {0, const_cast<char*>("svc_timescale"), {}},
        {0, const_cast<char*>(goldsrc::OFF_SVC_MESSAGES_P), {}}
    };

    template <class T>
    nitroapi::SearchConfig Address(T* pointer)
    {
        return nitroapi::SearchConfig(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer)));
    }

    class EngineAddressProvider final : public nitroapi::AddressProviderBase
    {
    private:
        std::unordered_map<nitroapi::StaticHookId, nitroapi::SearchConfig> addresses_ = {
            {nitroapi::StaticHookId::Host_Frame, Address(&Host)},
            {nitroapi::StaticHookId::Host_FrameInternal, Address(&InternalHost)},
            {nitroapi::StaticHookId::Host_FilterTime, Address(&FilterTime)}
        };
        std::unordered_map<std::string, nitroapi::SearchConfig> strings_ = {
            {"EngineMsgBase", Address(g_Messages)},
            {"Host_EndGame", Address(&EndGame)}
        };

    private:
        const std::unordered_map<nitroapi::StaticHookId, nitroapi::SearchConfig>& get_addresses_id() override
        {
            return addresses_;
        }

        const std::unordered_map<std::string, nitroapi::SearchConfig>& get_addresses_str() override
        {
            return strings_;
        }
    };

    class Observer final : public nitroapi::FrameLifecycleObserverInterface
    {
    private:
        std::vector<std::string>& events_;

    public:
        explicit Observer(std::vector<std::string>& events) :
            events_(events)
        {}

        void OnHostEntry() override
        {
            events_.emplace_back("entry");
        }

        void OnFrameAccepted() override
        {
            events_.emplace_back("accepted");
        }

        void OnHostExit(bool completed) override
        {
            events_.emplace_back(completed ? "exit" : "abort");
        }
    };

    class EngineFrameLifecycleTest : public testing::Test
    {
    private:
        std::shared_ptr<nitroapi::HookStorage> hooks_ = std::make_shared<nitroapi::HookStorage>();
        std::unique_ptr<nitroapi::EngineModule> module_;
        Observer observer_{events};
        Observer default_observer_{default_events};

    protected:
        nitroapi::FrameLifecycleService service;
        std::vector<std::string> events;
        std::vector<std::string> default_events;
        EngineFrame frame;
        std::shared_ptr<nitroapi::EngineData> data;

        void SetUp() override
        {
            g_Frame = &frame;
            service.Subscribe(&observer_);
            nitroapi::FrameLifecycle_GetDefaultService().Clear();
            nitroapi::FrameLifecycle_GetDefaultService().Subscribe(&default_observer_);

            module_ = std::make_unique<nitroapi::EngineModule>(
                hooks_,
                std::make_shared<EngineAddressProvider>(),
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                service
            );
            // A null module keeps offset lookups relative to zero for the in-process fixture.
            module_->OnLibraryLoaded(nullptr);
            data = module_->GetEngineData();
            ASSERT_TRUE(data->Host_Frame.AttachHook());
            ASSERT_TRUE(data->Host_FrameInternal.AttachHook());
            ASSERT_TRUE(data->Host_FilterTime.AttachHook());
        }

        void TearDown() override
        {
            service.Clear();
            nitroapi::FrameLifecycle_GetDefaultService().Clear();
            if (module_)
            {
                module_->OnLibraryUnload();
            }

            g_Frame = nullptr;
        }
    };
} // namespace

TEST_F(EngineFrameLifecycleTest, ExplicitOwnerPreservesArgumentsAndResults)
{
    int state_info = 0;
    EXPECT_EQ(data->Host_Frame.InvokeChained(0.125f, 7, &state_info), 7);
    EXPECT_EQ(state_info, 42);
    EXPECT_EQ(frame.filter_calls, 1);
    EXPECT_FLOAT_EQ(frame.filter_delta, 0.125f);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "accepted", "exit"}));
    EXPECT_TRUE(default_events.empty());

    events.clear();
    nitroapi::FrameLifecycle_InvokeHost(service, [&] { EXPECT_EQ(data->Host_FilterTime.InvokeChained(0.25f), 7); });
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "accepted", "exit"}));
    EXPECT_FLOAT_EQ(frame.filter_delta, 0.25f);
}

TEST_F(EngineFrameLifecycleTest, ExitFollowsTheCompleteHandlersAndSubscribersChain)
{
    std::shared_ptr<nitroapi::Unsubscriber> handler = data->Host_Frame |= [&](float delta, int state, int* info, auto* next) {
        events.emplace_back("handler.enter");
        int result = next->Invoke(delta, state, info);
        events.emplace_back("handler.exit");
        return result;
    };
    std::shared_ptr<nitroapi::Unsubscriber> subscriber = data->Host_Frame +=
        [&](float, int, int*, int) { events.emplace_back("subscriber"); };

    int state_info = 0;
    EXPECT_EQ(data->Host_Frame.InvokeChained(0.125f, 7, &state_info), 7);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "handler.enter", "accepted", "handler.exit", "subscriber", "exit"}));
}

TEST_F(EngineFrameLifecycleTest, SuppressingTheOriginalStillReportsTheOuterInvocation)
{
    std::shared_ptr<nitroapi::Unsubscriber> handler = data->Host_Frame |= [&](float, int, int*, auto*) {
        events.emplace_back("handler");
        return 13;
    };

    int state_info = 0;
    EXPECT_EQ(data->Host_Frame.InvokeChained(0.125f, 7, &state_info), 13);
    EXPECT_EQ(frame.body_calls, 0);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "handler", "exit"}));
}

TEST_F(EngineFrameLifecycleTest, RejectedFilterDoesNotAcceptTheFrame)
{
    frame.filter_result = 0;
    int state_info = 0;
    data->Host_Frame.InvokeChained(0.125f, 7, &state_info);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "exit"}));
}

TEST_F(EngineFrameLifecycleTest, ReattachingDoesNotDuplicateTheWrapper)
{
    for (int index = 0; index < 3; ++index)
    {
        events.clear();
        ASSERT_TRUE(data->Host_Frame.DetachHook());
        ASSERT_TRUE(data->Host_Frame.AttachHook());

        int state_info = 0;
        EXPECT_EQ(data->Host_Frame.InvokeChained(0.125f, 7, &state_info), 7);
        EXPECT_EQ(events, (std::vector<std::string>{"entry", "accepted", "exit"}));
    }
}

TEST_F(EngineFrameLifecycleTest, NestedInvocationsKeepTheirOwnBoundaries)
{
    bool nested = false;
    std::shared_ptr<nitroapi::Unsubscriber> handler = data->Host_Frame |= [&](float delta, int state, int* info, auto* next) {
        if (!nested)
        {
            nested = true;
            int nested_info = 0;
            EXPECT_EQ(data->Host_Frame.InvokeChained(delta, state + 1, &nested_info), state + 1);
        }

        return next->Invoke(delta, state, info);
    };

    int state_info = 0;
    data->Host_Frame.InvokeChained(0.125f, 7, &state_info);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "entry", "accepted", "exit", "accepted", "exit"}));
}

TEST_F(EngineFrameLifecycleTest, InternalAbortMarksOnlyTheExplicitOwnerAndNotTheNextFrame)
{
    frame.abort_mode = AbortMode::Internal;
    int state_info = 0;
    nitroapi::FrameLifecycle_InvokeHost(nitroapi::FrameLifecycle_GetDefaultService(), [&] {
        EXPECT_EQ(data->Host_Frame.InvokeChained(0.125f, 7, &state_info), 7);
    });

    EXPECT_EQ(state_info, 42);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "accepted", "abort"}));
    EXPECT_EQ(default_events, (std::vector<std::string>{"entry", "exit"}));

    frame.abort_mode = AbortMode::None;
    events.clear();
    data->Host_Frame.InvokeChained(0.25f, 7, &state_info);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "accepted", "exit"}));
}

TEST_F(EngineFrameLifecycleTest, OuterAbortDoesNotLoseSubsequentBoundaries)
{
    frame.abort_mode = AbortMode::Outer;
    int state_info = 0;
    EXPECT_EQ(data->Host_Frame.InvokeChained(0.125f, 7, &state_info), 8);
    EXPECT_EQ(state_info, 0);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "accepted", "abort"}));

    frame.abort_mode = AbortMode::None;
    events.clear();
    EXPECT_EQ(data->Host_Frame.InvokeChained(0.25f, 7, &state_info), 7);
    EXPECT_EQ(state_info, 42);
    EXPECT_EQ(events, (std::vector<std::string>{"entry", "accepted", "exit"}));
}
