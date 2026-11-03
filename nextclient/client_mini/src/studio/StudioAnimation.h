#pragma once

#include <array>
#include <memory>

#include <goldsrc/client_api.h>
#include <goldsrc/studio_api.h>
#include <nitroapi/modules/client/StudioAnimationData.h>
#include <ncl_math/Matrix3x4.h>

namespace studio
{
    class StudioAnimation
    {
    private:
        struct Pose
        {
            nitroapi::StudioPosition_t position[goldsrc::MAXSTUDIOBONES]{};
            nitroapi::StudioQuaternion_t quaternion[goldsrc::MAXSTUDIOBONES]{};
        };

        nitroapi::StudioAnimationData& data_;
        const goldsrc::engine_studio_api_t& studio_api_;
        const goldsrc::cl_enginefunc_t& engine_api_;
        std::array<std::shared_ptr<nitroapi::Unsubscriber>, 12> subscriptions_{};
        std::size_t subscription_count_{};
        bool started_{};
        bool direct_bone_quaternion_{};
        bool direct_bone_position_{};
        Pose base_[5]{};
        Pose game_[5]{};
        Pose merge_{};

    public:
        StudioAnimation(
            nitroapi::StudioAnimationData& data,
            const goldsrc::engine_studio_api_t& studio_api,
            const goldsrc::cl_enginefunc_t& engine_api
        );
        ~StudioAnimation();
        StudioAnimation(const StudioAnimation&) = delete;
        StudioAnimation& operator=(const StudioAnimation&) = delete;

        bool Start();
        void Stop();
        void StudioSetupBones(goldsrc::cstrike::CStudioModelRenderer* renderer);
        void GameStudioSetupBones(goldsrc::cstrike::CGameStudioModelRenderer* renderer);
        goldsrc::mstudioanim_t* StudioGetAnim(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            goldsrc::model_t* model,
            goldsrc::mstudioseqdesc_t* sequence
        );
        double StudioEstimateInterpolant(goldsrc::cstrike::CStudioModelRenderer* renderer);
        double StudioEstimateFrame(goldsrc::cstrike::CStudioModelRenderer* renderer, goldsrc::mstudioseqdesc_t* sequence);
        void StudioSlerpBones(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            nitroapi::StudioQuaternion_t* first_q,
            nitroapi::StudioPosition_t* first_p,
            nitroapi::StudioQuaternion_t* second_q,
            nitroapi::StudioPosition_t* second_p,
            float blend
        );
        void StudioCalcBoneAdj(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            float interpolant,
            float* adjustment,
            const uint8_t* current,
            const uint8_t* previous,
            uint8_t mouth
        );
        void StudioCalcBoneQuaterion(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            int frame,
            float fraction,
            goldsrc::mstudiobone_t* bone,
            goldsrc::mstudioanim_t* animation,
            float* adjustment,
            float* quaternion
        );
        void StudioCalcBonePosition(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            int frame,
            float fraction,
            goldsrc::mstudiobone_t* bone,
            goldsrc::mstudioanim_t* animation,
            float* adjustment,
            float* position
        );
        void StudioCalcRotations(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            nitroapi::StudioPosition_t* positions,
            nitroapi::StudioQuaternion_t* quaternions,
            goldsrc::mstudioseqdesc_t* sequence,
            goldsrc::mstudioanim_t* animation,
            float frame
        );
        void StudioSaveBones(goldsrc::cstrike::CStudioModelRenderer* renderer);
        void StudioMergeBones(goldsrc::cstrike::CStudioModelRenderer* renderer, goldsrc::model_t* model);

    private:
        template <class Function, class Result, class... Args>
        bool Subscribe(Function& function, Result (StudioAnimation::*method)(Args...), bool* direct_call = nullptr);
        void BuildMatrices(goldsrc::cstrike::CStudioModelRenderer* renderer, goldsrc::mstudiobone_t* bones, Pose& pose, bool mirror);
        void BuildBoneMatrix(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            int index,
            goldsrc::mstudiobone_t* bones,
            Pose& pose,
            bool mirror
        );
        void BlendBase(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            goldsrc::mstudioseqdesc_t* sequence,
            goldsrc::mstudioanim_t* animation,
            float frame,
            Pose& primary,
            bool previous
        );
        void BlendGame(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            goldsrc::mstudioseqdesc_t* sequence,
            goldsrc::mstudioanim_t* animation,
            float frame,
            Pose& primary,
            int first,
            int second,
            bool previous
        );
        goldsrc::mstudioanim_t* LookupAnimation(
            goldsrc::cstrike::CStudioModelRenderer* renderer,
            goldsrc::mstudioseqdesc_t* sequence,
            int group
        );

    private:
        static void CopyMatrix(const nitroapi::StudioMatrix_t& source, nitroapi::StudioMatrix_t& destination);
        static void ConcatMatrix(const nitroapi::StudioMatrix_t& left, const ncl_math::Matrix3x4& right, nitroapi::StudioMatrix_t& output);
    };
} // namespace studio
