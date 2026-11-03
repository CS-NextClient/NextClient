#include "StudioAnimation.h"

#include <cstring>
#include <type_traits>

#include <goldsrc/entity.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/model.h>
#include <ncl_studio/bone_animation.h>

namespace studio
{
    StudioAnimation::StudioAnimation(
        nitroapi::StudioAnimationData& data,
        const goldsrc::engine_studio_api_t& studio_api,
        const goldsrc::cl_enginefunc_t& engine_api
    ) :
        data_(data),
        studio_api_(studio_api),
        engine_api_(engine_api)
    {}

    StudioAnimation::~StudioAnimation()
    {
        Stop();
    }

    template <class Function, class Result, class... Args>
    bool StudioAnimation::Subscribe(Function& function, Result (StudioAnimation::*method)(Args...), bool* direct_call)
    {
        if (function.GetChain() == nullptr || function.GetHook() == nullptr)
        {
            return false;
        }

        nitroapi::HandlersChainInterface<Result, Args...>* chain = function.GetChain();
        std::function<Result(Args..., nitroapi::NextHandlerInterface<Result, Args...>*)> callback =
            [this, method](Args... args, nitroapi::NextHandlerInterface<Result, Args...>*) -> Result { return (this->*method)(args...); };
        std::shared_ptr<nitroapi::ProxyHandler<Result, Args...>> handler =
            std::make_shared<nitroapi::ProxyHandler<Result, Args...>>(callback);

        if (!chain->AddHandler(handler.get()))
        {
            return false;
        }

        std::shared_ptr<nitroapi::Unsubscriber> tracking;

        if constexpr (std::is_same_v<Function, nitroapi::StudioBoneFunction_t>)
        {
            if (direct_call != nullptr && data_.TrackBoneHandler != nullptr)
            {
                tracking = data_.TrackBoneHandler(data_, function, handler.get(), direct_call);
            }
        }

        subscriptions_[subscription_count_++] = std::make_shared<nitroapi::Unsubscriber>([chain, handler, tracking] {
            if (tracking != nullptr)
            {
                tracking->Unsubscribe();
            }

            chain->RemoveHandler(handler.get());
        });

        if (chain->GetHook() == nullptr)
        {
            function.AttachHook();
        }

        if (!function.GetHook()->IsHookActive())
        {
            function.DetachHook();

            return false;
        }

        return true;
    }

    bool StudioAnimation::Start()
    {
        if (started_)
        {
            return true;
        }

        if (!Subscribe(data_.StudioSetupBones, &StudioAnimation::StudioSetupBones) ||
            !Subscribe(data_.GameStudioSetupBones, &StudioAnimation::GameStudioSetupBones) ||
            !Subscribe(data_.StudioGetAnim, &StudioAnimation::StudioGetAnim) ||
            !Subscribe(data_.StudioEstimateInterpolant, &StudioAnimation::StudioEstimateInterpolant) ||
            !Subscribe(data_.StudioEstimateFrame, &StudioAnimation::StudioEstimateFrame) ||
            !Subscribe(data_.StudioSlerpBones, &StudioAnimation::StudioSlerpBones) ||
            !Subscribe(data_.StudioCalcBoneAdj, &StudioAnimation::StudioCalcBoneAdj) ||
            !Subscribe(data_.StudioCalcBoneQuaterion, &StudioAnimation::StudioCalcBoneQuaterion, &direct_bone_quaternion_) ||
            !Subscribe(data_.StudioCalcBonePosition, &StudioAnimation::StudioCalcBonePosition, &direct_bone_position_) ||
            !Subscribe(data_.StudioCalcRotations, &StudioAnimation::StudioCalcRotations) ||
            !Subscribe(data_.StudioSaveBones, &StudioAnimation::StudioSaveBones) ||
            !Subscribe(data_.StudioMergeBones, &StudioAnimation::StudioMergeBones))
        {
            Stop();

            return false;
        }

        started_ = true;

        return true;
    }

    void StudioAnimation::Stop()
    {
        started_ = false;
        direct_bone_quaternion_ = false;
        direct_bone_position_ = false;

        while (subscription_count_ != 0)
        {
            std::shared_ptr<nitroapi::Unsubscriber>& subscription = subscriptions_[--subscription_count_];
            subscription->Unsubscribe();
            subscription.reset();
        }
    }

    goldsrc::mstudioanim_t* StudioAnimation::StudioGetAnim(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        goldsrc::model_t* model,
        goldsrc::mstudioseqdesc_t* sequence
    )
    {
        goldsrc::studiohdr_t* header = *data_.GetRendererView(renderer).studio_header;
        goldsrc::mstudioseqgroup_t* group =
            reinterpret_cast<goldsrc::mstudioseqgroup_t*>(reinterpret_cast<std::byte*>(header) + header->seqgroupindex) +
            sequence->seqgroup;

        if (sequence->seqgroup == 0)
        {
            return reinterpret_cast<goldsrc::mstudioanim_t*>(reinterpret_cast<std::byte*>(header) + sequence->animindex);
        }

        // Studio sequence caches occupy the model's submodels pointer slot.
        goldsrc::cache_user_t* cache = reinterpret_cast<goldsrc::cache_user_t*>(model->submodels);

        if (cache == nullptr)
        {
            cache = static_cast<goldsrc::cache_user_t*>(studio_api_.Mem_Calloc(16, sizeof(goldsrc::cache_user_t)));
            model->submodels = reinterpret_cast<goldsrc::dmodel_t*>(cache);
        }

        if (studio_api_.Cache_Check(cache + sequence->seqgroup) == nullptr)
        {
            engine_api_.Con_DPrintf("loading %s\n", group->name);
            studio_api_.LoadCacheFile(group->name, cache + sequence->seqgroup);
        }

        return reinterpret_cast<goldsrc::mstudioanim_t*>(static_cast<std::byte*>(cache[sequence->seqgroup].data) + sequence->animindex);
    }

    double StudioAnimation::StudioEstimateInterpolant(goldsrc::cstrike::CStudioModelRenderer* renderer)
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        goldsrc::cl_entity_t* entity = *view.current_entity;
        double result = 1;

        if (*view.interpolate != 0 && double(entity->latched.prevanimtime) + 0.01 <= double(entity->curstate.animtime))
        {
            result = (*view.time - double(entity->curstate.animtime)) * 10.0;

            if (result > 2)
            {
                result = 2;
            }
        }

        return result;
    }

    double StudioAnimation::StudioEstimateFrame(goldsrc::cstrike::CStudioModelRenderer* renderer, goldsrc::mstudioseqdesc_t* sequence)
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        goldsrc::cl_entity_t* entity = *view.current_entity;
        double elapsed = 0;

        if (*view.interpolate != 0 && !(double(entity->curstate.animtime) > *view.time))
        {
            elapsed = (*view.time - double(entity->curstate.animtime)) * double(entity->curstate.framerate) * double(sequence->fps);
        }

        double frame = sequence->numframes > 1
                           ? double(sequence->numframes - 1) * double(entity->curstate.frame) * (1.0 / 256.0)
                           : 0.0;
        frame += elapsed;

        if ((sequence->flags & goldsrc::STUDIO_LOOPING) != 0)
        {
            if (sequence->numframes > 1)
            {
                int span = sequence->numframes - 1;
                frame -= int(frame / double(span)) * span;
            }

            if (frame < 0)
            {
                frame += sequence->numframes - 1;
            }
        }
        else
        {
            double upper = double(sequence->numframes) - 1.001;

            if (frame >= upper)
            {
                frame = upper;
            }

            if (frame < 0)
            {
                frame = 0;
            }
        }

        return frame;
    }

    void StudioAnimation::StudioSlerpBones(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        nitroapi::StudioQuaternion_t* first_q,
        nitroapi::StudioPosition_t* first_p,
        nitroapi::StudioQuaternion_t* second_q,
        nitroapi::StudioPosition_t* second_p,
        float blend
    )
    {
        if (blend < 0)
        {
            blend = 0;
        }

        if (blend > 1)
        {
            blend = 1;
        }

        float inverse = float(1.0 - double(blend));
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);

        for (int index = 0; index < (*view.studio_header)->numbones; ++index)
        {
            const ncl_math::Quaternion first = ncl_math::Quaternion::Load(first_q[index]);
            ncl_math::Quaternion second = ncl_math::Quaternion::Load(second_q[index]);
            ncl_math::Quaternion quaternion;

            ncl_math::QuaternionSlerp(first, second, blend, quaternion);
            second.Store(second_q[index]);
            quaternion.Store(first_q[index]);

            for (int axis = 0; axis < 3; ++axis)
            {
                first_p[index][axis] =
                    float(double(inverse) * double(first_p[index][axis]) + double(blend) * double(second_p[index][axis]));
            }
        }
    }

    void StudioAnimation::StudioCalcBoneAdj(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        float interpolant,
        float* adjustment,
        const uint8_t* current,
        const uint8_t* previous,
        uint8_t mouth
    )
    {
        ncl_studio::CalcBoneAdj(
            **data_.GetRendererView(renderer).studio_header,
            interpolant,
            adjustment,
            current,
            previous,
            mouth
        );
    }

    void StudioAnimation::StudioCalcBoneQuaterion(
        goldsrc::cstrike::CStudioModelRenderer*,
        int frame,
        float fraction,
        goldsrc::mstudiobone_t* bone,
        goldsrc::mstudioanim_t* animation,
        float* adjustment,
        float* quaternion
    )
    {
        ncl_math::Quaternion result;
        ncl_studio::CalcBoneQuaternion(frame, fraction, *bone, *animation, adjustment, result);
        result.Store(quaternion);
    }

    void StudioAnimation::StudioCalcBonePosition(
        goldsrc::cstrike::CStudioModelRenderer*,
        int frame,
        float fraction,
        goldsrc::mstudiobone_t* bone,
        goldsrc::mstudioanim_t* animation,
        float* adjustment,
        float* position
    )
    {
        ncl_math::Vector3 result;
        ncl_studio::CalcBonePosition(frame, fraction, *bone, *animation, adjustment, result);
        std::memcpy(position, result.data(), sizeof(result));
    }

    void StudioAnimation::StudioCalcRotations(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        nitroapi::StudioPosition_t* positions,
        nitroapi::StudioQuaternion_t* quaternions,
        goldsrc::mstudioseqdesc_t* sequence,
        goldsrc::mstudioanim_t* animation,
        float frame
    )
    {
        if (frame > sequence->numframes - 1)
        {
            frame = 0;
        }
        else if (double(frame) < -0.01)
        {
            frame = -0.01f;
        }

        int integer_frame = int(frame);
        float fraction = float(double(frame) - integer_frame);

        float interpolant = float(data_.StudioEstimateInterpolant.InvokeChained(renderer));

        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        goldsrc::studiohdr_t* header = *view.studio_header;
        goldsrc::mstudiobone_t* bones = reinterpret_cast<goldsrc::mstudiobone_t*>(reinterpret_cast<std::byte*>(header) + header->boneindex);
        goldsrc::cl_entity_t* entity = *view.current_entity;
        float adjustment[8];

        data_.StudioCalcBoneAdj.InvokeChained(
            renderer,
            interpolant,
            adjustment,
            entity->curstate.controller,
            entity->latched.prevcontroller,
            entity->mouth.mouthopen
        );

        for (int index = 0; index < (*view.studio_header)->numbones; ++index)
        {
            if (started_ && direct_bone_quaternion_ && data_.StudioCalcBoneQuaterion.GetHook()->IsHookActive())
            {
                StudioCalcBoneQuaterion(
                    renderer,
                    integer_frame,
                    fraction,
                    bones + index,
                    animation + index,
                    adjustment,
                    quaternions[index]
                );
            }
            else
            {
                data_.StudioCalcBoneQuaterion.InvokeChained(
                    renderer,
                    integer_frame,
                    fraction,
                    bones + index,
                    animation + index,
                    adjustment,
                    quaternions[index]
                );
            }

            if (started_ && direct_bone_position_ && data_.StudioCalcBonePosition.GetHook()->IsHookActive())
            {
                StudioCalcBonePosition(
                    renderer,
                    integer_frame,
                    fraction,
                    bones + index,
                    animation + index,
                    adjustment,
                    positions[index]
                );
            }
            else
            {
                data_.StudioCalcBonePosition.InvokeChained(
                    renderer,
                    integer_frame,
                    fraction,
                    bones + index,
                    animation + index,
                    adjustment,
                    positions[index]
                );
            }
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            if ((sequence->motiontype & (goldsrc::STUDIO_X << axis)) != 0)
            {
                positions[sequence->motionbone][axis] = 0;
            }
        }

        // Both stock clients use a positive-zero linear-motion coefficient.
        double factor =
            ((1.0 - (double(frame) - integer_frame)) / sequence->numframes) * double((*view.current_entity)->curstate.framerate) * 0.0;

        for (int axis = 0; axis < 3; ++axis)
        {
            if ((sequence->motiontype & (goldsrc::STUDIO_LX << axis)) != 0)
            {
                positions[sequence->motionbone][axis] =
                    float(double(positions[sequence->motionbone][axis]) + factor * double(sequence->linearmovement[axis]));
            }
        }
    }

    void StudioAnimation::StudioSaveBones(goldsrc::cstrike::CStudioModelRenderer* renderer)
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        goldsrc::studiohdr_t* header = *view.studio_header;
        goldsrc::mstudiobone_t* bones = reinterpret_cast<goldsrc::mstudiobone_t*>(reinterpret_cast<std::byte*>(header) + header->boneindex);

        *view.cached_bones = header->numbones;

        for (int index = 0; index < (*view.studio_header)->numbones; ++index)
        {
            std::strcpy(view.cached_names[index], bones[index].name);

            CopyMatrix((*view.bone_transform)[index], view.cached_bone_transform[index]);
            CopyMatrix((*view.light_transform)[index], view.cached_light_transform[index]);
        }
    }

    void StudioAnimation::CopyMatrix(const nitroapi::StudioMatrix_t& source, nitroapi::StudioMatrix_t& destination)
    {
        const ncl_math::Matrix3x4 matrix = ncl_math::Matrix3x4::Load(source);
        matrix.Store(destination);
    }
} // namespace studio
