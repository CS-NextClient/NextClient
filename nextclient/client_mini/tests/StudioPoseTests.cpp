#include <cstdarg>
#include <vector>

#include <gtest/gtest.h>

#include <goldsrc/cvar.h>

#include "StudioAnimationTestSupport.h"
#include "studio/StudioAnimation.h"

namespace
{
    struct CacheDiagnosticState
    {
        std::vector<int>* calls{};
        goldsrc::mstudioseqdesc_t* sequence{};
        const char* filename{};
    };

    CacheDiagnosticState g_CacheDiagnostic;

    void PrintCacheLoad(const char* format, ...)
    {
        g_CacheDiagnostic.calls->push_back(3);

        EXPECT_STREQ(format, "loading %s\n");

        va_list arguments;
        va_start(arguments, format);

        EXPECT_EQ(va_arg(arguments, char*), g_CacheDiagnostic.filename);

        va_end(arguments);
        g_CacheDiagnostic.sequence->seqgroup = 2;
    }

    class StudioPoseTests : public ::testing::Test
    {
    protected:
        studio::testing::Renderer renderer;
        nitroapi::StudioAnimationData data;
        goldsrc::engine_studio_api_t studio_api{};
        goldsrc::cl_enginefunc_t engine_api{};
        studio::testing::FunctionBinding<decltype(data.StudioSetupBones)> studio_setup_bones;
        studio::testing::FunctionBinding<decltype(data.GameStudioSetupBones)> game_studio_setup_bones;
        studio::testing::FunctionBinding<decltype(data.StudioGetAnim)> studio_get_anim;
        studio::testing::FunctionBinding<decltype(data.StudioEstimateInterpolant)> studio_estimate_interpolant;
        studio::testing::FunctionBinding<decltype(data.StudioEstimateFrame)> studio_estimate_frame;
        studio::testing::FunctionBinding<decltype(data.StudioSlerpBones)> studio_slerp_bones;
        studio::testing::FunctionBinding<decltype(data.StudioCalcBoneAdj)> studio_calc_bone_adj;
        studio::testing::FunctionBinding<decltype(data.StudioCalcBoneQuaterion)> studio_calc_bone_quaterion;
        studio::testing::FunctionBinding<decltype(data.StudioCalcBonePosition)> studio_calc_bone_position;
        studio::testing::FunctionBinding<decltype(data.StudioCalcRotations)> studio_calc_rotations;
        studio::testing::FunctionBinding<decltype(data.StudioSaveBones)> studio_save_bones;
        studio::testing::FunctionBinding<decltype(data.StudioMergeBones)> studio_merge_bones;
        studio::StudioAnimation owner{data, studio_api, engine_api};

        void SetUp() override
        {
            studio_setup_bones.Bind(data.StudioSetupBones, owner, &studio::StudioAnimation::StudioSetupBones);
            game_studio_setup_bones.Bind(data.GameStudioSetupBones, owner, &studio::StudioAnimation::GameStudioSetupBones);
            studio_get_anim.Bind(data.StudioGetAnim, owner, &studio::StudioAnimation::StudioGetAnim);
            studio_estimate_interpolant.Bind(data.StudioEstimateInterpolant, owner, &studio::StudioAnimation::StudioEstimateInterpolant);
            studio_estimate_frame.Bind(data.StudioEstimateFrame, owner, &studio::StudioAnimation::StudioEstimateFrame);
            studio_slerp_bones.Bind(data.StudioSlerpBones, owner, &studio::StudioAnimation::StudioSlerpBones);
            studio_calc_bone_adj.Bind(data.StudioCalcBoneAdj, owner, &studio::StudioAnimation::StudioCalcBoneAdj);
            studio_calc_bone_quaterion.Bind(data.StudioCalcBoneQuaterion, owner, &studio::StudioAnimation::StudioCalcBoneQuaterion);
            studio_calc_bone_position.Bind(data.StudioCalcBonePosition, owner, &studio::StudioAnimation::StudioCalcBonePosition);
            studio_calc_rotations.Bind(data.StudioCalcRotations, owner, &studio::StudioAnimation::StudioCalcRotations);
            studio_save_bones.Bind(data.StudioSaveBones, owner, &studio::StudioAnimation::StudioSaveBones);
            studio_merge_bones.Bind(data.StudioMergeBones, owner, &studio::StudioAnimation::StudioMergeBones);
            data.GetRendererView = studio::testing::Renderer::View;
            data.GetGameRendererView = studio::testing::Renderer::GameView;
            studio_api.IsHardware = [] { return 1; };
            engine_api.GetViewModel = []() -> goldsrc::cl_entity_t* { return nullptr; };
            data.StudioFxTransform = [](goldsrc::cstrike::CStudioModelRenderer*, goldsrc::cl_entity_t*, nitroapi::StudioMatrix_t* matrix) {
                (*matrix)[0][3] += 100;
            };
            static goldsrc::cvar_t* right_hand = nullptr;
            static uint8_t shield = 0;
            static int is_czero = 0;
            data.right_hand = &right_hand;
            data.shield = &shield;
            data.is_czero = &is_czero;
        }

        void UseGroupPositions()
        {
            studio_calc_rotations.hook.original = [&](auto*, auto* positions, auto* quaternions, auto*, auto* animation, float) {
                int group = int(animation - renderer.storage.animations[0]) / renderer.header->numbones;

                for (int index = 0; index < renderer.header->numbones; ++index)
                {
                    positions[index][0] = float(group * 10);
                    positions[index][1] = 0;
                    positions[index][2] = 0;
                    quaternions[index][0] = 0;
                    quaternions[index][1] = 0;
                    quaternions[index][2] = 0;
                    quaternions[index][3] = 1;
                }
            };
        }
    };

    TEST_F(StudioPoseTests, InterpolantPreservesNegativeAndCapsOnlyUpperBound)
    {
        renderer.interpolate = 1;
        renderer.entity.curstate.animtime = 1;
        renderer.entity.latched.prevanimtime = 0;
        renderer.time = 0.75;

        EXPECT_DOUBLE_EQ(owner.StudioEstimateInterpolant(renderer.receiver()), -2.5);

        renderer.time = 2;

        EXPECT_DOUBLE_EQ(owner.StudioEstimateInterpolant(renderer.receiver()), 2);

        renderer.interpolate = 0;

        EXPECT_DOUBLE_EQ(owner.StudioEstimateInterpolant(renderer.receiver()), 1);
    }

    TEST_F(StudioPoseTests, FrameWrapsNegativeLoopAndClampsNonloop)
    {
        goldsrc::mstudioseqdesc_t& sequence = renderer.storage.sequences[0];
        sequence.flags = goldsrc::STUDIO_LOOPING;
        renderer.entity.curstate.frame = -128;

        EXPECT_DOUBLE_EQ(owner.StudioEstimateFrame(renderer.receiver(), &sequence), 1.5);

        sequence.flags = 0;

        EXPECT_DOUBLE_EQ(owner.StudioEstimateFrame(renderer.receiver(), &sequence), 0);

        renderer.entity.curstate.frame = 256;

        EXPECT_DOUBLE_EQ(owner.StudioEstimateFrame(renderer.receiver(), &sequence), 2.999);
    }

    TEST_F(StudioPoseTests, SlerpClampsEndpointsAndMutatesSecondQuaternion)
    {
        renderer.header->numbones = 1;
        nitroapi::StudioQuaternion_t first_q[1]{{0, 0, 0, 1}};
        nitroapi::StudioQuaternion_t second_q[1]{{0, 0, 0, -1}};
        nitroapi::StudioPosition_t first_p[1]{{0, 4, 8}};
        nitroapi::StudioPosition_t second_p[1]{{8, 0, 4}};
        owner.StudioSlerpBones(renderer.receiver(), first_q, first_p, second_q, second_p, -1);

        EXPECT_FLOAT_EQ(second_q[0][3], 1);
        EXPECT_FLOAT_EQ(first_p[0][0], 0);

        owner.StudioSlerpBones(renderer.receiver(), first_q, first_p, second_q, second_p, 0.25f);

        EXPECT_FLOAT_EQ(first_p[0][0], 2);
        EXPECT_FLOAT_EQ(first_p[0][1], 3);
        EXPECT_FLOAT_EQ(first_p[0][2], 7);

        owner.StudioSlerpBones(renderer.receiver(), first_q, first_p, second_q, second_p, 2.0f);

        EXPECT_FLOAT_EQ(first_p[0][0], 8);
        EXPECT_FLOAT_EQ(first_p[0][1], 0);
        EXPECT_FLOAT_EQ(first_p[0][2], 4);
    }

    TEST_F(StudioPoseTests, RotationsPreserveCallOrderNegativeFractionAndZeroLinearMotion)
    {
        std::vector<int> calls;
        studio_estimate_interpolant.hook.before = [&](auto*) { calls.push_back(1); };
        studio_calc_bone_adj.hook.before = [&](auto*, float, float*, const uint8_t*, const uint8_t*, uint8_t) { calls.push_back(2); };
        studio_calc_bone_quaterion.hook.before = [&](auto*, int frame, float fraction, auto*, auto*, float*, float*) {
            calls.push_back(3);

            EXPECT_EQ(frame, 0);
            EXPECT_FLOAT_EQ(fraction, -0.01f);
        };
        studio_calc_bone_position.hook.before = [&](auto*, int, float, auto*, auto*, float*, float*) { calls.push_back(4); };
        goldsrc::mstudioseqdesc_t& sequence = renderer.storage.sequences[0];
        sequence.motiontype = 0x1c5;
        sequence.linearmovement[0] = 100;
        sequence.linearmovement[1] = 200;
        sequence.linearmovement[2] = 300;
        renderer.entity.curstate.framerate = 2;
        nitroapi::StudioPosition_t positions[2]{};
        nitroapi::StudioQuaternion_t quaternions[2]{};
        owner.StudioCalcRotations(
            renderer.receiver(),
            positions,
            quaternions,
            &sequence,
            renderer.storage.animations[0],
            -1
        );

        EXPECT_EQ(calls, (std::vector<int>{1, 2, 3, 4, 3, 4}));
        EXPECT_FLOAT_EQ(positions[0][0], 0);
        EXPECT_FLOAT_EQ(positions[0][1], 2);
        EXPECT_FLOAT_EQ(positions[0][2], 0);
        EXPECT_FLOAT_EQ(positions[1][0], 4);
        EXPECT_FLOAT_EQ(quaternions[0][3], 1);
    }

    TEST_F(StudioPoseTests, BaseHierarchyCopiesLightBeforeRootFxAndUsesIndependentChildOutputs)
    {
        std::vector<int> calls;
        studio_estimate_frame.hook.before = [&](auto*, auto*) { calls.push_back(1); };
        studio_get_anim.hook.before = [&](auto*, auto*, auto*) { calls.push_back(2); };
        owner.StudioSetupBones(renderer.receiver());

        ASSERT_GE(calls.size(), 2u);
        EXPECT_EQ(calls[0], 1);
        EXPECT_EQ(calls[1], 2);
        EXPECT_FLOAT_EQ(renderer.bones[0][0][3], 111);
        EXPECT_FLOAT_EQ(renderer.bones[0][1][3], 22);
        EXPECT_FLOAT_EQ(renderer.bones[1][0][3], 115);
        EXPECT_FLOAT_EQ(renderer.bones[1][1][3], 27);
        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 11);
        EXPECT_FLOAT_EQ(renderer.lights[1][0][3], 15);
    }

    TEST_F(StudioPoseTests, BaseUsesAliasTransformForSoftwareAndGameNonplayerCallsBase)
    {
        studio_api.IsHardware = [] { return 0; };
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_FLOAT_EQ(renderer.bones[0][0][3], 1101);
        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 11);
        EXPECT_FLOAT_EQ(renderer.bones[1][0][3], 1105);
    }

    TEST_F(StudioPoseTests, SaveAndMergeMatchCaseInsensitiveNamesAndFallbackChildUsesMatchedParent)
    {
        renderer.bones[0][0][0] = 1;
        renderer.bones[0][1][1] = 1;
        renderer.bones[0][2][2] = 1;
        renderer.bones[0][0][3] = 77;
        renderer.lights[0][0][0] = 1;
        renderer.lights[0][1][1] = 1;
        renderer.lights[0][2][2] = 1;
        renderer.lights[0][0][3] = 88;
        owner.StudioSaveBones(renderer.receiver());

        EXPECT_EQ(renderer.cached_count, 2);
        EXPECT_STREQ(renderer.names[0], "root");

        std::strcpy(renderer.storage.bones[0].name, "ROOT");
        std::strcpy(renderer.storage.bones[1].name, "unmatched");
        owner.StudioMergeBones(renderer.receiver(), &renderer.model);

        EXPECT_FLOAT_EQ(renderer.bones[0][0][3], 77);
        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 88);
        EXPECT_FLOAT_EQ(renderer.bones[1][0][3], 81);
        EXPECT_FLOAT_EQ(renderer.lights[1][0][3], 92);
    }

    TEST_F(StudioPoseTests, BaseFourWayBlendReadsChangedCurrentByteAfterFirstSlerp)
    {
        UseGroupPositions();
        renderer.header->numbones = 1;
        renderer.storage.sequences[0].numblends = 4;
        renderer.entity.curstate.blending[0] = 0;
        renderer.entity.curstate.blending[1] = 255;
        int slerps = 0;
        studio_slerp_bones.hook.before = [&](auto*, auto*, auto*, auto*, auto*, float) {
            if (slerps++ == 0)
            {
                renderer.entity.curstate.blending[0] = 255;
            }
        };
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_EQ(slerps, 3);
        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 40);

        renderer.storage.sequences[0].numblends = 2;
        renderer.entity.curstate.blending[0] = 255;
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 20);
    }

    TEST_F(StudioPoseTests, BaseTransitionUsesPreviousSequenceAtStartAndClosesAtBoundary)
    {
        renderer.header->numbones = 1;
        renderer.interpolate = 1;
        renderer.entity.latched.sequencetime = 4;
        renderer.entity.latched.prevsequence = 1;
        renderer.entity.curstate.frame = 128;
        renderer.time = 4;
        studio_calc_rotations.hook.original = [&](auto*, auto* positions, auto* quaternions, auto* sequence, auto*, float) {
            positions[0][0] = sequence == &renderer.storage.sequences[0] ? 0 : 20;
            positions[0][1] = 0;
            positions[0][2] = 0;
            quaternions[0][0] = 0;
            quaternions[0][1] = 0;
            quaternions[0][2] = 0;
            quaternions[0][3] = 1;
        };
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 30);

        renderer.time = 4.1;
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 20);

        renderer.time = 4.2;
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 10);
        EXPECT_FLOAT_EQ(renderer.entity.latched.prevframe, 1.5f);
    }

    TEST_F(StudioPoseTests, PreviousSequenceReloadsFrameBeforeEachBlendedGroup)
    {
        UseGroupPositions();
        renderer.header->numbones = 1;
        renderer.interpolate = 1;
        renderer.time = 4;
        renderer.entity.latched.sequencetime = 4;
        renderer.entity.latched.prevsequence = 1;
        renderer.entity.latched.prevframe = 5;
        renderer.storage.sequences[1].numblends = 4;
        std::vector<float> frames;
        studio_calc_rotations.hook.before = [&](auto*, auto*, auto*, auto* sequence, auto*, float frame) {
            if (sequence == &renderer.storage.sequences[1])
            {
                frames.push_back(frame);
                renderer.entity.latched.prevframe += 1;
            }
        };
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_EQ(frames, (std::vector<float>{5, 6, 7, 8}));

        frames.clear();
        renderer.entity.player = 1;
        renderer.entity.index = 1;
        renderer.entity.curstate.sequence = 101;
        renderer.player_info = &renderer.player;
        renderer.entity.latched.prevframe = 5;
        renderer.storage.sequences[1].numblends = 9;
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_EQ(frames, (std::vector<float>{5, 6, 7, 8}));
    }

    TEST_F(StudioPoseTests, GameNineWaySelectsEveryGridCellAndClampsHighEndpoint)
    {
        UseGroupPositions();
        renderer.header->numbones = 1;
        renderer.entity.player = 1;
        renderer.entity.index = 1;
        renderer.entity.curstate.sequence = 101;
        renderer.player_info = &renderer.player;
        renderer.storage.sequences[101].numblends = 9;
        struct Case
        {
            uint8_t first;
            uint8_t second;
            int groups[4];
            int calls;
            float expected;
        };
        const Case cases[] = {
            {0, 0, {0, 1, 3, 4}, 4, 10},
            {127, 0, {0, 1, 3, 4}, 4, 10 + 2540.0f / 255},
            {128, 0, {1, 2, 4, 5}, 5, 20 + 20.0f / 255},
            {0, 128, {3, 4, 6, 7}, 5, 40 + 60.0f / 255},
            {255, 255, {4, 5, 7, 8}, 5, 90},
        };

        for (const Case& value : cases)
        {
            std::vector<int> groups;
            int lookups = 0;
            studio_get_anim.hook.before = [&](auto*, auto*, auto*) { ++lookups; };
            studio_calc_rotations.hook.before = [&](auto*, auto*, auto*, auto*, auto* animation, float) {
                groups.push_back(int(animation - renderer.storage.animations[0]));
            };
            renderer.entity.curstate.blending[0] = value.first;
            renderer.entity.curstate.blending[1] = value.second;
            owner.GameStudioSetupBones(renderer.game_receiver());

            EXPECT_EQ(groups, (std::vector<int>(value.groups, value.groups + 4)));
            EXPECT_EQ(lookups, value.calls);
            EXPECT_NEAR(renderer.lights[0][0][3], value.expected, 0.00001f);
        }
    }

    TEST_F(StudioPoseTests, GameLookupUsesLiveBlendCountAndOldGridCapturesBytesBeforeGetAnim)
    {
        UseGroupPositions();
        renderer.header->numbones = 1;
        renderer.entity.player = 1;
        renderer.entity.index = 1;
        renderer.entity.curstate.sequence = 101;
        renderer.player_info = &renderer.player;
        renderer.storage.sequences[101].numblends = 9;
        renderer.entity.curstate.blending[0] = 255;
        renderer.entity.curstate.blending[1] = 255;
        int lookups = 0;
        studio_get_anim.hook.before = [&](auto*, auto*, auto*) {
            if (++lookups == 2)
            {
                renderer.storage.sequences[101].numblends = 1;
            }
        };
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_EQ(lookups, 5);
        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 10);

        renderer.interpolate = 1;
        renderer.time = 4;
        renderer.entity.latched.sequencetime = 4;
        renderer.entity.latched.prevsequence = 102;
        renderer.entity.latched.prevseqblending[0] = 255;
        renderer.entity.latched.prevseqblending[1] = 255;
        renderer.storage.sequences[102].numblends = 9;
        studio_get_anim.hook.before = [&](auto*, auto*, auto* sequence) {
            if (sequence == &renderer.storage.sequences[102])
            {
                renderer.entity.latched.prevseqblending[0] = 0;
                renderer.entity.latched.prevseqblending[1] = 0;
            }
        };
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 90);
    }

    TEST_F(StudioPoseTests, GameGaitCacheResetPrecedesFrameAndGaitThreeChangesBlend)
    {
        renderer.header->numbones = 1;
        renderer.entity.player = 1;
        renderer.entity.index = 1;
        renderer.entity.curstate.sequence = 101;
        renderer.player_info = &renderer.player;
        renderer.player.gaitsequence = 6;
        renderer.player.gaitframe = 3;
        std::vector<int> calls;
        studio_get_anim.hook.before = [&](auto*, auto*, auto*) { calls.push_back(1); };
        studio_estimate_frame.hook.before = [&](auto*, auto*) {
            calls.push_back(2);

            EXPECT_FLOAT_EQ(renderer.player.gaitframe, 0);
            EXPECT_EQ(renderer.gait[1], 6);
        };
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_EQ(calls, (std::vector<int>{1, 2}));

        renderer.player.gaitframe = 3;
        studio_estimate_frame.hook.before = {};
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_FLOAT_EQ(renderer.player.gaitframe, 3);

        renderer.player.gaitsequence = 3;
        renderer.entity.curstate.blending[0] = 26;
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_EQ(renderer.entity.curstate.blending[0], 0);
        EXPECT_EQ(renderer.entity.latched.prevseqblending[0], 0);

        renderer.entity.curstate.blending[0] = 27;
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_EQ(renderer.entity.curstate.blending[0], 1);
        EXPECT_EQ(renderer.entity.latched.prevseqblending[0], 1);
    }

    TEST_F(StudioPoseTests, OwnerRemovesPartialSubscriptionsAndRestoresOriginalAfterStop)
    {
        studio_calc_bone_position.hook.fail = true;

        EXPECT_FALSE(owner.Start());
        EXPECT_EQ(studio_setup_bones.chain.GetHandlersCount(), 0u);
        EXPECT_EQ(studio_calc_bone_quaterion.chain.GetHandlersCount(), 0u);
        EXPECT_EQ(studio_calc_bone_position.chain.GetHandlersCount(), 0u);

        owner.Stop();
        studio_calc_bone_position.hook.fail = false;

        EXPECT_TRUE(owner.Start());
        EXPECT_TRUE(owner.Start());

        int original_calls = 0;
        studio_estimate_interpolant.hook.original = [&](auto*) {
            ++original_calls;

            return 42.0;
        };

        EXPECT_DOUBLE_EQ(data.StudioEstimateInterpolant.InvokeChained(renderer.receiver()), 1);
        EXPECT_EQ(original_calls, 0);

        owner.Stop();
        owner.Stop();

        EXPECT_DOUBLE_EQ(data.StudioEstimateInterpolant.InvokeChained(renderer.receiver()), 42);
        EXPECT_EQ(original_calls, 1);
    }

    TEST_F(StudioPoseTests, OwnerRollsBackWhenChainRejectsHandlerAndCanRestart)
    {
        nitroapi::ProxyHandler<
            void,
            goldsrc::cstrike::CStudioModelRenderer*,
            int,
            float,
            goldsrc::mstudiobone_t*,
            goldsrc::mstudioanim_t*,
            float*,
            float*>
            external([](auto*, int, float, auto*, auto*, float*, float*, auto*) {});

        ASSERT_TRUE(studio_calc_bone_position.chain.AddHandler(&external));

        studio_calc_bone_position.chain.reject_handler = true;

        EXPECT_FALSE(owner.Start());
        EXPECT_EQ(studio_setup_bones.chain.GetHandlersCount(), 0u);
        EXPECT_EQ(studio_calc_bone_quaterion.chain.GetHandlersCount(), 0u);
        EXPECT_EQ(studio_calc_bone_position.chain.handler, &external);
        EXPECT_EQ(studio_calc_rotations.chain.GetHandlersCount(), 0u);

        owner.Stop();

        EXPECT_EQ(studio_calc_bone_position.chain.handler, &external);
        ASSERT_TRUE(studio_calc_bone_position.chain.RemoveHandler(&external));

        studio_calc_bone_position.chain.reject_handler = false;

        ASSERT_TRUE(owner.Start());
        EXPECT_EQ(studio_calc_bone_position.chain.GetHandlersCount(), 1u);

        owner.Stop();

        EXPECT_EQ(studio_calc_bone_position.chain.GetHandlersCount(), 0u);
    }

    TEST_F(StudioPoseTests, BaseMirrorsRootIncludingTranslationAndKeepsHardwareCallOrder)
    {
        static goldsrc::cl_entity_t* viewmodel;
        static int hardware_calls;
        viewmodel = &renderer.entity;
        hardware_calls = 0;
        engine_api.GetViewModel = [] { return viewmodel; };
        studio_api.IsHardware = [] {
            ++hardware_calls;

            return 1;
        };
        goldsrc::cvar_t right_hand{};
        right_hand.value = 1;
        goldsrc::cvar_t* right_hand_ptr = &right_hand;
        uint8_t shield = 0;
        int is_czero = 0;
        data.right_hand = &right_hand_ptr;
        data.shield = &shield;
        data.is_czero = &is_czero;
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][1][3], 18);
        EXPECT_FLOAT_EQ(renderer.lights[1][1][3], 13);
        EXPECT_EQ(hardware_calls, 2);

        hardware_calls = 0;
        shield = 1;
        is_czero = 1;
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][1][3], 18);
        EXPECT_EQ(hardware_calls, 3);
    }

    TEST_F(StudioPoseTests, BaseGaitStopsAtSpineAndGameMaskReenablesAtPelvisChild)
    {
        renderer.header->numbones = 4;
        renderer.header->boneindex += sizeof(goldsrc::mstudiobone_t);
        goldsrc::mstudiobone_t* bones = renderer.storage.bones + 1;
        std::strcpy(renderer.storage.bones[0].name, "sentinel");
        std::strcpy(bones[0].name, "Bip01 Pelvis");
        bones[0].parent = -1;
        std::strcpy(bones[1].name, "Bip01 Spine");
        bones[1].parent = 0;
        std::strcpy(bones[2].name, "leg");
        bones[2].parent = 0;
        std::strcpy(bones[3].name, "unrelated");
        bones[3].parent = 1;
        renderer.player_info = &renderer.player;
        renderer.player.gaitsequence = 1;
        studio_calc_rotations.hook.original = [&](auto*, auto* positions, auto* quaternions, auto* sequence, auto*, float) {
            for (int index = 0; index < 4; ++index)
            {
                positions[index][0] = sequence == &renderer.storage.sequences[1] ? 10.0f : 1.0f;
                positions[index][1] = 0;
                positions[index][2] = 0;
                quaternions[index][0] = 0;
                quaternions[index][1] = 0;
                quaternions[index][2] = 0;
                quaternions[index][3] = 1;
            }
        };
        owner.StudioSetupBones(renderer.receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 20);
        EXPECT_FLOAT_EQ(renderer.lights[1][0][3], 21);
        EXPECT_FLOAT_EQ(renderer.lights[2][0][3], 21);
        EXPECT_FLOAT_EQ(renderer.lights[3][0][3], 22);

        renderer.entity.player = 1;
        renderer.entity.index = 1;
        owner.GameStudioSetupBones(renderer.game_receiver());

        EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 20);
        EXPECT_FLOAT_EQ(renderer.lights[1][0][3], 21);
        EXPECT_FLOAT_EQ(renderer.lights[2][0][3], 30);
        EXPECT_FLOAT_EQ(renderer.lights[3][0][3], 31);
    }

    TEST_F(StudioPoseTests, GaitCallbacksKeepCapturedBoneDescriptionsAndReadLiveCount)
    {
        renderer.storage.header.boneindex += sizeof(goldsrc::mstudiobone_t);
        goldsrc::mstudiobone_t* bones = renderer.storage.bones + 1;
        std::strcpy(renderer.storage.bones[0].name, "sentinel");
        std::strcpy(bones[0].name, "Bip01 Pelvis");
        std::strcpy(bones[1].name, "Bip01 Spine");
        std::strcpy(bones[2].name, "leg");
        bones[0].parent = -1;
        bones[1].parent = 0;
        bones[2].parent = 0;
        studio::testing::Renderer::ModelStorage alternate = renderer.storage;
        alternate.header.numbones = 3;

        for (goldsrc::mstudiobone_t& bone : alternate.bones)
        {
            std::strcpy(bone.name, "alternate");
            bone.parent = -1;
        }

        renderer.player_info = &renderer.player;
        renderer.player.gaitsequence = 1;
        renderer.entity.index = 1;
        studio_calc_rotations.hook.original = [&](auto*, auto* positions, auto* quaternions, auto* sequence, auto*, float) {
            for (int index = 0; index < 3; ++index)
            {
                positions[index][0] = sequence == &renderer.storage.sequences[1] ? 10.0f : 1.0f;
                positions[index][1] = 0;
                positions[index][2] = 0;
                quaternions[index][0] = 0;
                quaternions[index][1] = 0;
                quaternions[index][2] = 0;
                quaternions[index][3] = 1;
            }
        };

        for (bool during_lookup : {false, true})
        {
            auto replace_header = [&](auto* sequence) {
                if (sequence == &renderer.storage.sequences[1])
                {
                    renderer.header = &alternate.header;
                }
            };
            studio_get_anim.hook.before = [&](auto*, auto*, auto* sequence) {
                if (during_lookup)
                {
                    replace_header(sequence);
                }
            };
            studio_calc_rotations.hook.before = [&](auto*, auto*, auto*, auto* sequence, auto*, float) {
                if (!during_lookup)
                {
                    replace_header(sequence);
                }
            };

            for (int player : {0, 1})
            {
                renderer.header = &renderer.storage.header;
                renderer.entity.player = player;

                if (player == 0)
                {
                    owner.StudioSetupBones(renderer.receiver());
                }
                else
                {
                    owner.GameStudioSetupBones(renderer.game_receiver());
                }

                EXPECT_EQ(renderer.header, &alternate.header);
                EXPECT_FLOAT_EQ(renderer.lights[0][0][3], 20);
                EXPECT_FLOAT_EQ(renderer.lights[1][0][3], 21);
                EXPECT_FLOAT_EQ(renderer.lights[2][0][3], player == 0 ? 21 : 30);
            }
        }
    }

    TEST_F(StudioPoseTests, GameOverlayGateControlsInvalidGaitCorrection)
    {
        renderer.header->numbones = 0;
        renderer.entity.player = 1;
        renderer.entity.index = 1;
        renderer.player_info = &renderer.player;
        const int sequences[] = {8, 9, 101, 100};

        for (int sequence : sequences)
        {
            renderer.entity.curstate.sequence = sequence;
            renderer.player.gaitsequence = -1;
            int lookups = 0;
            studio_get_anim.hook.before = [&](auto*, auto*, auto*) { ++lookups; };
            owner.GameStudioSetupBones(renderer.game_receiver());

            EXPECT_EQ(lookups, sequence == 100 ? 2 : 1);
            EXPECT_EQ(renderer.player.gaitsequence, sequence == 100 ? 0 : -1);
        }
    }

    TEST_F(StudioPoseTests, GetAnimAllocatesCacheAndKeepsCapturedStorageAcrossCallbacks)
    {
        static goldsrc::cache_user_t cache[16];
        static goldsrc::cache_user_t replacement[16];
        static std::byte data_bytes[256];
        static goldsrc::model_t* model;
        static goldsrc::mstudioseqdesc_t* sequence;
        static std::vector<int> calls;
        static char* filename;
        std::memset(cache, 0, sizeof(cache));
        std::memset(replacement, 0, sizeof(replacement));
        calls.clear();
        model = &renderer.model;
        sequence = &renderer.storage.sequences[0];
        sequence->seqgroup = 1;
        sequence->animindex = 12;
        filename = renderer.storage.groups[1].name;
        std::strcpy(filename, "external.mdl");
        cache[2].data = data_bytes;
        studio_api.Mem_Calloc = [](int count, size_t size) -> void* {
            calls.push_back(1);

            EXPECT_EQ(count, 16);
            EXPECT_EQ(size, 4u);

            return cache;
        };
        studio_api.Cache_Check = [](goldsrc::cache_user_t* entry) -> void* {
            calls.push_back(2);

            EXPECT_EQ(entry, cache + 1);

            model->submodels = reinterpret_cast<goldsrc::dmodel_t*>(replacement);

            return nullptr;
        };
        g_CacheDiagnostic = {&calls, sequence, filename};
        engine_api.Con_DPrintf = PrintCacheLoad;
        studio_api.LoadCacheFile = [](char* name, goldsrc::cache_user_t* entry) {
            calls.push_back(4);

            EXPECT_EQ(name, filename);
            EXPECT_EQ(entry, cache + 2);

            sequence->animindex = 24;
        };

        EXPECT_EQ(owner.StudioGetAnim(renderer.receiver(), model, sequence), reinterpret_cast<goldsrc::mstudioanim_t*>(data_bytes + 24));
        EXPECT_EQ(calls, (std::vector<int>{1, 2, 3, 4}));
        EXPECT_EQ(model->submodels, reinterpret_cast<goldsrc::dmodel_t*>(replacement));

        sequence->seqgroup = 0;
        sequence->animindex = 12;

        EXPECT_EQ(
            owner.StudioGetAnim(renderer.receiver(), model, sequence),
            reinterpret_cast<goldsrc::mstudioanim_t*>(reinterpret_cast<std::byte*>(renderer.header) + 12)
        );

        EXPECT_EQ(calls.size(), 4u);

        g_CacheDiagnostic = {};
    }
} // namespace
