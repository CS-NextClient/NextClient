#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <goldsrc/cstrike/studio_renderer.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/fwd.h>

#include <nitroapi/NitroFunction.h>

namespace nitroapi
{
    using StudioMatrix_t = float[3][4];
    using StudioPosition_t = float[3];
    using StudioQuaternion_t = float[4];
    using StudioBoneFunction_t = NitroFunctionVoid<
        goldsrc::cstrike::CStudioModelRenderer*,
        int,
        float,
        goldsrc::mstudiobone_t*,
        goldsrc::mstudioanim_t*,
        float*,
        float*>;
    using StudioBoneHandler_t = HandlerInterface<
        void,
        goldsrc::cstrike::CStudioModelRenderer*,
        int,
        float,
        goldsrc::mstudiobone_t*,
        goldsrc::mstudioanim_t*,
        float*,
        float*>;

    struct StudioRendererView
    {
        double* time{};
        int* interpolate{};
        goldsrc::cl_entity_t** current_entity{};
        goldsrc::model_t** render_model{};
        goldsrc::player_info_t** player_info{};
        goldsrc::studiohdr_t** studio_header{};
        int* cached_bones{};
        char (*cached_names)[32]{};
        StudioMatrix_t* cached_bone_transform{};
        StudioMatrix_t* cached_light_transform{};
        StudioMatrix_t** rotation_matrix{};
        StudioMatrix_t** alias_transform{};
        StudioMatrix_t** bone_transform{};
        StudioMatrix_t** light_transform{};
    };

    struct StudioGameRendererView
    {
        StudioRendererView base;
        std::byte* gait_history{};

        int get_previous_gait(int entity_index) const
        {
            int value;
            std::memcpy(&value, gait_history + entity_index * sizeof(int), sizeof(value));

            return value;
        }

        void SetPreviousGait(int entity_index, int value) const
        {
            std::memcpy(gait_history + entity_index * sizeof(int), &value, sizeof(value));
        }
    };

    struct StudioAnimationData
    {
        NitroFunctionVoid<goldsrc::cstrike::CStudioModelRenderer*> StudioSetupBones;
        NitroFunctionVoid<goldsrc::cstrike::CGameStudioModelRenderer*> GameStudioSetupBones;

        NitroFunction<
            goldsrc::mstudioanim_t*,
            goldsrc::cstrike::CStudioModelRenderer*,
            goldsrc::model_t*,
            goldsrc::mstudioseqdesc_t*>
            StudioGetAnim;

        // Windows leaves extended ST0 results; binary64 transport preserves the caller's first store.
        NitroFunction<double, goldsrc::cstrike::CStudioModelRenderer*> StudioEstimateInterpolant;
        NitroFunction<double, goldsrc::cstrike::CStudioModelRenderer*, goldsrc::mstudioseqdesc_t*> StudioEstimateFrame;

        NitroFunctionVoid<
            goldsrc::cstrike::CStudioModelRenderer*,
            StudioQuaternion_t*,
            StudioPosition_t*,
            StudioQuaternion_t*,
            StudioPosition_t*,
            float>
            StudioSlerpBones;

        NitroFunctionVoid<
            goldsrc::cstrike::CStudioModelRenderer*,
            float,
            float*,
            const uint8_t*,
            const uint8_t*,
            uint8_t>
            StudioCalcBoneAdj;

        StudioBoneFunction_t StudioCalcBoneQuaterion;
        StudioBoneFunction_t StudioCalcBonePosition;

        NitroFunctionVoid<
            goldsrc::cstrike::CStudioModelRenderer*,
            StudioPosition_t*,
            StudioQuaternion_t*,
            goldsrc::mstudioseqdesc_t*,
            goldsrc::mstudioanim_t*,
            float>
            StudioCalcRotations;

        NitroFunctionVoid<goldsrc::cstrike::CStudioModelRenderer*> StudioSaveBones;
        NitroFunctionVoid<goldsrc::cstrike::CStudioModelRenderer*, goldsrc::model_t*> StudioMergeBones;

        StudioRendererView (*GetRendererView)(goldsrc::cstrike::CStudioModelRenderer*){};
        StudioGameRendererView (*GetGameRendererView)(goldsrc::cstrike::CGameStudioModelRenderer*){};
        void (*StudioFxTransform)(goldsrc::cstrike::CStudioModelRenderer*, goldsrc::cl_entity_t*, StudioMatrix_t*){};
        goldsrc::cvar_t** right_hand{};
        const uint8_t* shield{};
        const int* is_czero{};
        // The state flag remains borrowed until the returned subscription is explicitly removed.
        std::shared_ptr<Unsubscriber> (*TrackBoneHandler)(
            StudioAnimationData&,
            StudioBoneFunction_t&,
            StudioBoneHandler_t*,
            bool*
        ){};
    };
} // namespace nitroapi
