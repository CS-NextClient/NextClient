#include <functional>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include <nitroapi/ChainExtensions.h>
#include <nitroapi/modules/client/StudioAnimationData.h>

#include "chain/HandlersChain.h"
#include "modules/client/studio_animation.h"

namespace
{
    template <class... Args>
    class TestHook final : public nitroapi::GenericHookInterface<void, Args...>
    {
    private:
        std::function<void(Args...)> callback_;

    public:
        void SetCallbackAndEnableHook(std::function<void(Args...)>&& callback) override
        {
            callback_ = std::move(callback);
        }

        void UnsetCallbackAndDisableHook() override
        {
            callback_ = {};
        }

        void InvokeOriginal(Args...) override {}

        void Invoke(Args... args) override
        {
            callback_(args...);
        }

        uint32_t GetTrampolineAddress() const override
        {
            return 2;
        }

        uint32_t GetOriginalFunctionAddress() const override
        {
            return 1;
        }

        bool IsHookActive() const override
        {
            return bool(callback_);
        }
    };
} // namespace

TEST(StudioDirectCall, RequiresTheTrackedHandlerIdentityAndNoSubscribers)
{
    bool direct = false;
    nitroapi::HandlersChain<void, int> chain;
    TestHook<int> hook;
    nitroapi::ProxyHandler<void, int> owner([](int, auto*) {});
    nitroapi::ProxyHandler<void, int> foreign([](int, auto*) {});
    nitroapi::ProxySubscriber<void, int> observer([](int) {});

    chain.AttachHook(&hook);

    EXPECT_FALSE(chain.TrackSingleHandler(&owner, &direct));
    EXPECT_FALSE(direct);
    ASSERT_TRUE(chain.AddHandler(&owner));
    ASSERT_TRUE(chain.TrackSingleHandler(&owner, &direct));
    EXPECT_TRUE(direct);

    ASSERT_TRUE(chain.AddHandler(&foreign));
    EXPECT_FALSE(direct);
    ASSERT_TRUE(chain.RemoveHandler(&owner));
    EXPECT_EQ(chain.GetHandlersCount(), 1u);
    EXPECT_FALSE(direct);
    ASSERT_TRUE(chain.RemoveHandler(&foreign));
    ASSERT_TRUE(chain.AddHandler(&owner));
    EXPECT_TRUE(direct);

    ASSERT_TRUE(chain.Subscribe(&observer));
    EXPECT_FALSE(direct);
    ASSERT_TRUE(chain.Unsubscribe(&observer));
    EXPECT_TRUE(direct);
    ASSERT_TRUE(chain.TrackSingleHandler(nullptr, &direct));
    EXPECT_FALSE(direct);

    chain.RemoveHandler(&owner);
    chain.DetachHook();
}

TEST(StudioDirectCall, BindingChangesAndTrackingOwnershipInvalidateDirectCalls)
{
    bool direct = false;
    bool other = false;
    nitroapi::HandlersChain<void, int> chain;
    TestHook<int> hook;
    nitroapi::ProxyHandler<void, int> owner([](int, auto*) {});
    chain.AddHandler(&owner);

    ASSERT_TRUE(chain.TrackSingleHandler(&owner, &direct));
    EXPECT_FALSE(direct);

    chain.AttachHook(&hook);

    EXPECT_TRUE(direct);
    EXPECT_FALSE(chain.TrackSingleHandler(&owner, &other));
    EXPECT_FALSE(chain.TrackSingleHandler(nullptr, &other));
    EXPECT_TRUE(direct);

    chain.DetachHook();

    EXPECT_FALSE(direct);

    chain.AttachHook(&hook);

    EXPECT_TRUE(direct);
    ASSERT_TRUE(chain.TrackSingleHandler(nullptr, &direct));

    chain.RemoveHandler(&owner);
    chain.DetachHook();
}

TEST(StudioDirectCall, ReentrantObserverRemovalUpdatesTheNextCallImmediately)
{
    bool direct = false;
    nitroapi::HandlersChain<void, int> chain;
    TestHook<int> hook;
    int observed = 0;
    nitroapi::ProxySubscriber<void, int> observer([&](int) { ++observed; });
    nitroapi::ProxyHandler<void, int> owner([&](int, auto*) {
        EXPECT_FALSE(direct);

        chain.Unsubscribe(&observer);

        EXPECT_TRUE(direct);
    });
    chain.AddHandler(&owner);
    chain.AttachHook(&hook);

    ASSERT_TRUE(chain.TrackSingleHandler(&owner, &direct));

    chain.Subscribe(&observer);
    hook.Invoke(1);

    EXPECT_EQ(observed, 0);
    EXPECT_TRUE(direct);

    chain.TrackSingleHandler(nullptr, &direct);
    chain.RemoveHandler(&owner);
    chain.DetachHook();
}

TEST(StudioDirectCall, ProviderTrackingKeepsItsOriginalChainWhenTheFunctionIsRebound)
{
    nitroapi::StudioAnimationData data;
    nitroapi::StudioAnimation_InitializeAccess(data);
#if USE_PROFILER
    EXPECT_EQ(data.TrackBoneHandler, nullptr);
#else
    ASSERT_NE(data.TrackBoneHandler, nullptr);

    using Chain_t = nitroapi::HandlersChain<
        void,
        goldsrc::cstrike::CStudioModelRenderer*,
        int,
        float,
        goldsrc::mstudiobone_t*,
        goldsrc::mstudioanim_t*,
        float*,
        float*>;
    using Hook_t =
        TestHook<goldsrc::cstrike::CStudioModelRenderer*, int, float, goldsrc::mstudiobone_t*, goldsrc::mstudioanim_t*, float*, float*>;
    class BoneHandler final : public nitroapi::StudioBoneHandler_t
    {
    public:
        void Handle(
            goldsrc::cstrike::CStudioModelRenderer*,
            int,
            float,
            goldsrc::mstudiobone_t*,
            goldsrc::mstudioanim_t*,
            float*,
            float*,
            nitroapi::NextHandlerInterface<
                void,
                goldsrc::cstrike::CStudioModelRenderer*,
                int,
                float,
                goldsrc::mstudiobone_t*,
                goldsrc::mstudioanim_t*,
                float*,
                float*>*
        ) override
        {}
    };

    bool direct = false;
    Chain_t original;
    Chain_t replacement;
    Hook_t hook;
    BoneHandler owner;
    data.StudioCalcBonePosition.SetChain(&original);
    data.StudioCalcBonePosition.SetHook(&hook);
    original.AddHandler(&owner);
    auto tracking = data.TrackBoneHandler(data, data.StudioCalcBonePosition, &owner, &direct);

    ASSERT_NE(tracking, nullptr);

    data.StudioCalcBonePosition.AttachHook();

    EXPECT_TRUE(direct);

    data.StudioCalcBonePosition.SetChain(&replacement);

    EXPECT_FALSE(direct);

    tracking->Unsubscribe();
    original.AttachHook(&hook);

    EXPECT_FALSE(direct);

    nitroapi::StudioBoneFunction_t foreign;
    foreign.SetChain(&original);
    foreign.SetHook(&hook);

    EXPECT_EQ(data.TrackBoneHandler(data, foreign, &owner, &direct), nullptr);

    original.RemoveHandler(&owner);
    original.DetachHook();
#endif
}
