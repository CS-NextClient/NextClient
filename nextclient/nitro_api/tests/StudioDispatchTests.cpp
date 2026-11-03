#include <gtest/gtest.h>

#include "StudioAnimationTestSupport.h"
#include "chain/HandlersChain.h"
#include "modules/client/studio_animation.h"
#include "studio/StudioAnimation.h"

namespace
{
    template <class Function>
    struct BoneBinding;

    template <class... Args>
    struct BoneBinding<nitroapi::NitroFunctionVoid<Args...>>
    {
        nitroapi::HandlersChain<void, Args...> chain;
        studio::testing::Hook<void, Args...> hook;

        void Bind(
            nitroapi::NitroFunctionVoid<Args...>& function,
            studio::StudioAnimation& owner,
            void (studio::StudioAnimation::*method)(Args...)
        )
        {
            function.SetChain(&chain);
            function.SetHook(&hook);
            hook.original = [&owner, method](Args... args) { (owner.*method)(args...); };
        }
    };

    class StudioDispatch : public ::testing::Test
    {
    protected:
        studio::testing::Renderer renderer;
        nitroapi::StudioAnimationData data;
        goldsrc::engine_studio_api_t studio_api{};
        goldsrc::cl_enginefunc_t engine_api{};
        studio::testing::FunctionBinding<decltype(data.StudioSetupBones)> setup;
        studio::testing::FunctionBinding<decltype(data.GameStudioSetupBones)> game_setup;
        studio::testing::FunctionBinding<decltype(data.StudioGetAnim)> get_anim;
        studio::testing::FunctionBinding<decltype(data.StudioEstimateInterpolant)> interpolant;
        studio::testing::FunctionBinding<decltype(data.StudioEstimateFrame)> frame;
        studio::testing::FunctionBinding<decltype(data.StudioSlerpBones)> slerp;
        studio::testing::FunctionBinding<decltype(data.StudioCalcBoneAdj)> adjustment;
        BoneBinding<decltype(data.StudioCalcBoneQuaterion)> quaternion;
        BoneBinding<decltype(data.StudioCalcBonePosition)> position;
        studio::testing::FunctionBinding<decltype(data.StudioCalcRotations)> rotations;
        studio::testing::FunctionBinding<decltype(data.StudioSaveBones)> save;
        studio::testing::FunctionBinding<decltype(data.StudioMergeBones)> merge;
        studio::StudioAnimation owner{data, studio_api, engine_api};
        nitroapi::StudioPosition_t positions[goldsrc::MAXSTUDIOBONES]{};
        nitroapi::StudioQuaternion_t quaternions[goldsrc::MAXSTUDIOBONES]{};
        int quaternion_calls{};
        int position_calls{};

        void SetUp() override
        {
            nitroapi::StudioAnimation_InitializeAccess(data);
            data.GetRendererView = studio::testing::Renderer::View;
            data.GetGameRendererView = studio::testing::Renderer::GameView;
            setup.Bind(data.StudioSetupBones, owner, &studio::StudioAnimation::StudioSetupBones);
            game_setup.Bind(data.GameStudioSetupBones, owner, &studio::StudioAnimation::GameStudioSetupBones);
            get_anim.Bind(data.StudioGetAnim, owner, &studio::StudioAnimation::StudioGetAnim);
            interpolant.Bind(data.StudioEstimateInterpolant, owner, &studio::StudioAnimation::StudioEstimateInterpolant);
            frame.Bind(data.StudioEstimateFrame, owner, &studio::StudioAnimation::StudioEstimateFrame);
            slerp.Bind(data.StudioSlerpBones, owner, &studio::StudioAnimation::StudioSlerpBones);
            adjustment.Bind(data.StudioCalcBoneAdj, owner, &studio::StudioAnimation::StudioCalcBoneAdj);
            quaternion.Bind(data.StudioCalcBoneQuaterion, owner, &studio::StudioAnimation::StudioCalcBoneQuaterion);
            position.Bind(data.StudioCalcBonePosition, owner, &studio::StudioAnimation::StudioCalcBonePosition);
            rotations.Bind(data.StudioCalcRotations, owner, &studio::StudioAnimation::StudioCalcRotations);
            save.Bind(data.StudioSaveBones, owner, &studio::StudioAnimation::StudioSaveBones);
            merge.Bind(data.StudioMergeBones, owner, &studio::StudioAnimation::StudioMergeBones);
            quaternion.hook.before = [&](auto*, int, float, auto*, auto*, float*, float*) { ++quaternion_calls; };
            position.hook.before = [&](auto*, int, float, auto*, auto*, float*, float*) { ++position_calls; };

            ASSERT_TRUE(owner.Start());
        }

        void TearDown() override
        {
            owner.Stop();
            quaternion.chain.DetachHook();
            position.chain.DetachHook();
        }

        void Calculate()
        {
            owner.StudioCalcRotations(
                renderer.receiver(), positions, quaternions, renderer.storage.sequences, renderer.storage.animations[0], 0.5f
            );
        }

        int DirectPathHookCalls() const
        {
            return data.TrackBoneHandler == nullptr ? renderer.header->numbones : 0;
        }
    };
} // namespace

TEST_F(StudioDispatch, DirectCallsSkipHooksUntilAnObserverIsRegistered)
{
    Calculate();

    EXPECT_EQ(quaternion_calls, DirectPathHookCalls());
    EXPECT_EQ(position_calls, DirectPathHookCalls());

    int observed = 0;
    auto subscription = data.StudioCalcBonePosition += [&](auto*, int, float, auto*, auto*, float*, float* output) {
        ++observed;
        output[0] = 37;
    };
    quaternion_calls = position_calls = 0;
    Calculate();

    EXPECT_EQ(quaternion_calls, DirectPathHookCalls());
    EXPECT_EQ(position_calls, renderer.header->numbones);
    EXPECT_EQ(observed, renderer.header->numbones);
    EXPECT_FLOAT_EQ(positions[0][0], 37);

    subscription->Unsubscribe();
    quaternion_calls = position_calls = 0;
    Calculate();

    EXPECT_EQ(quaternion_calls, DirectPathHookCalls());
    EXPECT_EQ(position_calls, DirectPathHookCalls());
}

TEST_F(StudioDispatch, MutationBetweenRotationAndPositionAffectsTheSameBone)
{
    int quaternion_observed = 0;
    int position_observed = 0;
    std::shared_ptr<nitroapi::Unsubscriber> quaternion_observer;
    std::shared_ptr<nitroapi::Unsubscriber> position_observer;
    quaternion_observer = data.StudioCalcBoneQuaterion += [&](auto*, int, float, auto*, auto*, float*, float*) {
        ++quaternion_observed;
        position_observer = data.StudioCalcBonePosition += [&](auto*, int, float, auto*, auto*, float*, float* output) {
            ++position_observed;
            output[0] = 71;
            quaternion_observer->Unsubscribe();
            position_observer->Unsubscribe();
        };
    };

    Calculate();

    EXPECT_EQ(quaternion_observed, 1);
    EXPECT_EQ(position_observed, 1);
    EXPECT_FLOAT_EQ(positions[0][0], 71);
    EXPECT_EQ(quaternion_calls, data.TrackBoneHandler == nullptr ? renderer.header->numbones : 1);
    EXPECT_EQ(position_calls, data.TrackBoneHandler == nullptr ? renderer.header->numbones : 1);
}

TEST_F(StudioDispatch, DirectHookDisableFallsBackAndStopReleasesTrackedState)
{
    quaternion.hook.UnsetCallbackAndDisableHook();
    Calculate();

    EXPECT_EQ(quaternion_calls, renderer.header->numbones);
    EXPECT_EQ(position_calls, DirectPathHookCalls());

    owner.Stop();
    quaternion_calls = position_calls = 0;
    Calculate();

    EXPECT_EQ(quaternion_calls, renderer.header->numbones);
    EXPECT_EQ(position_calls, renderer.header->numbones);

    quaternion.chain.DetachHook();

    ASSERT_TRUE(owner.Start());

    quaternion_calls = position_calls = 0;
    Calculate();

    EXPECT_EQ(quaternion_calls, DirectPathHookCalls());
    EXPECT_EQ(position_calls, DirectPathHookCalls());
}
