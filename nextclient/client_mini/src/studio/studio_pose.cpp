#include "StudioAnimation.h"

#include <cstring>
#ifndef _WIN32
#include <strings.h>
#endif

#include <goldsrc/cvar.h>
#include <goldsrc/entity.h>
#include <goldsrc/engine/client_state.h>
#include <ncl_math/Matrix3x4.h>

namespace
{
    goldsrc::mstudioseqdesc_t* Sequence(goldsrc::studiohdr_t* header, int index)
    {
        return reinterpret_cast<goldsrc::mstudioseqdesc_t*>(reinterpret_cast<std::byte*>(header) + header->seqindex) + index;
    }

    goldsrc::mstudiobone_t* Bones(goldsrc::studiohdr_t* header)
    {
        return reinterpret_cast<goldsrc::mstudiobone_t*>(reinterpret_cast<std::byte*>(header) + header->boneindex);
    }

    bool HasTransition(const nitroapi::StudioRendererView& view)
    {
        goldsrc::cl_entity_t* entity = *view.current_entity;

        return *view.interpolate != 0 &&
               entity->latched.sequencetime != 0 &&
               double(entity->latched.sequencetime) + 0.2 > *view.time &&
               entity->latched.prevsequence < (*view.studio_header)->numseq;
    }
} // namespace

namespace studio
{
    void StudioAnimation::BlendBase(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        goldsrc::mstudioseqdesc_t* sequence,
        goldsrc::mstudioanim_t* animation,
        float frame,
        Pose& primary,
        bool previous
    )
    {
        if (sequence->numblends <= 1)
        {
            return;
        }

        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);

        animation += (*view.studio_header)->numbones;

        data_.StudioCalcRotations.InvokeChained(
            renderer,
            base_[1].position,
            base_[1].quaternion,
            sequence,
            animation,
            previous ? (*view.current_entity)->latched.prevframe : frame
        );

        float interpolant = previous ? 1.0f : float(data_.StudioEstimateInterpolant.InvokeChained(renderer));
        double inverse = 1.0 - double(interpolant);
        goldsrc::cl_entity_t* entity = *view.current_entity;
        float blend = previous
                          ? float(double(entity->latched.prevseqblending[0]) * (1.0 / 255.0))
                          : float(
                                (double(entity->curstate.blending[0]) * interpolant + double(entity->latched.prevblending[0]) * inverse) *
                                (1.0 / 255.0)
                            );

        data_.StudioSlerpBones.InvokeChained(
            renderer,
            primary.quaternion,
            primary.position,
            base_[1].quaternion,
            base_[1].position,
            blend
        );

        if (sequence->numblends == 4)
        {
            animation += (*view.studio_header)->numbones;

            data_.StudioCalcRotations.InvokeChained(
                renderer,
                base_[2].position,
                base_[2].quaternion,
                sequence,
                animation,
                previous ? (*view.current_entity)->latched.prevframe : frame
            );

            animation += (*view.studio_header)->numbones;

            data_.StudioCalcRotations.InvokeChained(
                renderer,
                base_[3].position,
                base_[3].quaternion,
                sequence,
                animation,
                previous ? (*view.current_entity)->latched.prevframe : frame
            );

            entity = *view.current_entity;
            blend = previous
                        ? float(double(entity->latched.prevseqblending[0]) * (1.0 / 255.0))
                        : float(
                              (double(entity->curstate.blending[0]) * interpolant + double(entity->latched.prevblending[0]) * inverse) *
                              (1.0 / 255.0)
                          );

            data_.StudioSlerpBones.InvokeChained(
                renderer,
                base_[2].quaternion,
                base_[2].position,
                base_[3].quaternion,
                base_[3].position,
                blend
            );

            entity = *view.current_entity;
            blend = previous
                        ? float(double(entity->latched.prevseqblending[1]) * (1.0 / 255.0))
                        : float(
                              (double(entity->curstate.blending[1]) * interpolant + double(entity->latched.prevblending[1]) * inverse) *
                              (1.0 / 255.0)
                          );

            data_.StudioSlerpBones.InvokeChained(
                renderer,
                primary.quaternion,
                primary.position,
                base_[2].quaternion,
                base_[2].position,
                blend
            );
        }
    }

    void StudioAnimation::StudioSetupBones(goldsrc::cstrike::CStudioModelRenderer* renderer)
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        goldsrc::cl_entity_t* entity = *view.current_entity;

        if (entity->curstate.sequence < 0 || entity->curstate.sequence >= (*view.studio_header)->numseq)
        {
            entity->curstate.sequence = 0;
        }

        goldsrc::mstudioseqdesc_t* sequence = Sequence(*view.studio_header, entity->curstate.sequence);
        double frame = data_.StudioEstimateFrame.InvokeChained(renderer, sequence);
        goldsrc::mstudioanim_t* animation = data_.StudioGetAnim.InvokeChained(renderer, *view.render_model, sequence);

        data_.StudioCalcRotations.InvokeChained(
            renderer,
            base_[0].position,
            base_[0].quaternion,
            sequence,
            animation,
            float(frame)
        );

        BlendBase(
            renderer,
            sequence,
            animation,
            float(frame),
            base_[0],
            false
        );

        if (HasTransition(view))
        {
            entity = *view.current_entity;
            sequence = Sequence(*view.studio_header, entity->latched.prevsequence);
            animation = data_.StudioGetAnim.InvokeChained(renderer, *view.render_model, sequence);
            float previous_frame = (*view.current_entity)->latched.prevframe;

            data_.StudioCalcRotations.InvokeChained(
                renderer,
                base_[4].position,
                base_[4].quaternion,
                sequence,
                animation,
                previous_frame
            );

            BlendBase(
                renderer,
                sequence,
                animation,
                previous_frame,
                base_[4],
                true
            );

            float blend = float(1.0 - (*view.time - double((*view.current_entity)->latched.sequencetime)) * 5.0);

            data_.StudioSlerpBones.InvokeChained(
                renderer,
                base_[0].quaternion,
                base_[0].position,
                base_[4].quaternion,
                base_[4].position,
                blend
            );
        }
        else
        {
            (*view.current_entity)->latched.prevframe = float(frame);
        }

        goldsrc::mstudiobone_t* bones = Bones(*view.studio_header);

        if (*view.player_info != nullptr && (*view.player_info)->gaitsequence != 0)
        {
            sequence = Sequence(*view.studio_header, (*view.player_info)->gaitsequence);
            animation = data_.StudioGetAnim.InvokeChained(renderer, *view.render_model, sequence);

            data_.StudioCalcRotations.InvokeChained(
                renderer,
                base_[1].position,
                base_[1].quaternion,
                sequence,
                animation,
                (*view.player_info)->gaitframe
            );

            for (int index = 0; index < (*view.studio_header)->numbones; ++index)
            {
                if (std::strcmp(bones[index].name, "Bip01 Spine") == 0)
                {
                    break;
                }

                std::memcpy(base_[0].position[index], base_[1].position[index], sizeof(nitroapi::StudioPosition_t));
                std::memcpy(base_[0].quaternion[index], base_[1].quaternion[index], sizeof(nitroapi::StudioQuaternion_t));
            }
        }

        BuildMatrices(renderer, bones, base_[0], true);
    }

    goldsrc::mstudioanim_t* StudioAnimation::LookupAnimation(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        goldsrc::mstudioseqdesc_t* sequence,
        int group
    )
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        goldsrc::mstudioanim_t* animation = data_.StudioGetAnim.InvokeChained(renderer, *view.render_model, sequence);

        if (group >= 0 && group < sequence->numblends)
        {
            animation += (*view.studio_header)->numbones * group;
        }

        return animation;
    }

    void StudioAnimation::BlendGame(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        goldsrc::mstudioseqdesc_t* sequence,
        goldsrc::mstudioanim_t* animation,
        float frame,
        Pose& primary,
        int first,
        int second,
        bool previous
    )
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);

        if (sequence->numblends != 9)
        {
            data_.StudioCalcRotations.InvokeChained(
                renderer,
                primary.position,
                primary.quaternion,
                sequence,
                animation,
                previous ? (*view.current_entity)->latched.prevframe : frame
            );

            return;
        }

        int column = first > 127 ? 1 : 0;
        int row = second > 127 ? 1 : 0;
        int group = row * 3 + column;
        float horizontal = float(double(2 * (first - column * 127)) * (1.0 / 255.0));
        float vertical = float(double(2 * (second - row * 127)) * (1.0 / 255.0));

        if (group != 0)
        {
            animation = LookupAnimation(renderer, sequence, group);
        }

        data_.StudioCalcRotations.InvokeChained(
            renderer,
            primary.position,
            primary.quaternion,
            sequence,
            animation,
            previous ? (*view.current_entity)->latched.prevframe : frame
        );

        animation = LookupAnimation(renderer, sequence, group + 1);

        data_.StudioCalcRotations.InvokeChained(
            renderer,
            game_[1].position,
            game_[1].quaternion,
            sequence,
            animation,
            previous ? (*view.current_entity)->latched.prevframe : frame
        );

        animation = LookupAnimation(renderer, sequence, group + 3);

        data_.StudioCalcRotations.InvokeChained(
            renderer,
            game_[2].position,
            game_[2].quaternion,
            sequence,
            animation,
            previous ? (*view.current_entity)->latched.prevframe : frame
        );

        animation = LookupAnimation(renderer, sequence, group + 4);

        data_.StudioCalcRotations.InvokeChained(
            renderer,
            game_[3].position,
            game_[3].quaternion,
            sequence,
            animation,
            previous ? (*view.current_entity)->latched.prevframe : frame
        );

        data_.StudioSlerpBones.InvokeChained(
            renderer,
            primary.quaternion,
            primary.position,
            game_[1].quaternion,
            game_[1].position,
            horizontal
        );

        data_.StudioSlerpBones.InvokeChained(
            renderer,
            game_[2].quaternion,
            game_[2].position,
            game_[3].quaternion,
            game_[3].position,
            horizontal
        );

        data_.StudioSlerpBones.InvokeChained(
            renderer,
            primary.quaternion,
            primary.position,
            game_[2].quaternion,
            game_[2].position,
            vertical
        );
    }

    void StudioAnimation::GameStudioSetupBones(goldsrc::cstrike::CGameStudioModelRenderer* game_renderer)
    {
        nitroapi::StudioGameRendererView game_view = data_.GetGameRendererView(game_renderer);
        nitroapi::StudioRendererView view = game_view.base;
        goldsrc::cstrike::CStudioModelRenderer* renderer = reinterpret_cast<goldsrc::cstrike::CStudioModelRenderer*>(game_renderer);
        goldsrc::cl_entity_t* entity = *view.current_entity;

        if (entity->player == 0)
        {
            data_.StudioSetupBones.InvokeChained(renderer);

            return;
        }

        if (entity->curstate.sequence < 0 || entity->curstate.sequence >= (*view.studio_header)->numseq)
        {
            entity->curstate.sequence = 0;
        }

        goldsrc::mstudioseqdesc_t* sequence = Sequence(*view.studio_header, entity->curstate.sequence);
        goldsrc::mstudioanim_t* animation = data_.StudioGetAnim.InvokeChained(renderer, *view.render_model, sequence);

        if (*view.player_info != nullptr)
        {
            int entity_index = (*view.current_entity)->index;

            if (game_view.get_previous_gait(entity_index) != 6 && (*view.player_info)->gaitsequence == 6)
            {
                (*view.player_info)->gaitframe = 0;
            }

            game_view.SetPreviousGait(entity_index, (*view.player_info)->gaitsequence);
        }

        double frame = data_.StudioEstimateFrame.InvokeChained(renderer, sequence);

        if ((*view.player_info)->gaitsequence == 3)
        {
            entity = *view.current_entity;
            entity->curstate.blending[0] = entity->curstate.blending[0] <= 26 ? 0 : entity->curstate.blending[0] - 26;
            entity->latched.prevseqblending[0] = entity->curstate.blending[0];
        }

        entity = *view.current_entity;

        BlendGame(
            renderer,
            sequence,
            animation,
            float(frame),
            game_[0],
            entity->curstate.blending[0],
            entity->curstate.blending[1],
            false
        );

        if (HasTransition(view))
        {
            entity = *view.current_entity;
            int first = entity->latched.prevseqblending[0];
            int second = entity->latched.prevseqblending[1];
            sequence = Sequence(*view.studio_header, entity->latched.prevsequence);
            animation = data_.StudioGetAnim.InvokeChained(renderer, *view.render_model, sequence);

            BlendGame(
                renderer,
                sequence,
                animation,
                (*view.current_entity)->latched.prevframe,
                game_[4],
                first,
                second,
                true
            );

            float blend = float(1.0 - (*view.time - double((*view.current_entity)->latched.sequencetime)) * 5.0);

            data_.StudioSlerpBones.InvokeChained(
                renderer,
                game_[0].quaternion,
                game_[0].position,
                game_[4].quaternion,
                game_[4].position,
                blend
            );
        }
        else
        {
            (*view.current_entity)->latched.prevframe = float(frame);
        }

        goldsrc::mstudiobone_t* bones = Bones(*view.studio_header);
        entity = *view.current_entity;

        if (*view.player_info != nullptr &&
            entity->curstate.sequence < 101 &&
            entity->curstate.sequence != 8 &&
            entity->curstate.sequence != 9)
        {
            if ((*view.player_info)->gaitsequence < 0 || (*view.player_info)->gaitsequence >= (*view.studio_header)->numseq)
            {
                (*view.player_info)->gaitsequence = 0;
            }

            sequence = Sequence(*view.studio_header, (*view.player_info)->gaitsequence);
            animation = data_.StudioGetAnim.InvokeChained(renderer, *view.render_model, sequence);

            data_.StudioCalcRotations.InvokeChained(
                renderer,
                game_[1].position,
                game_[1].quaternion,
                sequence,
                animation,
                (*view.player_info)->gaitframe
            );

            bool use_gait = true;

            for (int index = 0; index < (*view.studio_header)->numbones; ++index)
            {
                if (std::strcmp(bones[index].name, "Bip01 Spine") == 0)
                {
                    use_gait = false;
                    continue;
                }

                if (std::strcmp(bones[bones[index].parent].name, "Bip01 Pelvis") == 0)
                {
                    use_gait = true;
                }

                if (use_gait)
                {
                    std::memcpy(game_[0].position[index], game_[1].position[index], sizeof(nitroapi::StudioPosition_t));
                    std::memcpy(game_[0].quaternion[index], game_[1].quaternion[index], sizeof(nitroapi::StudioQuaternion_t));
                }
            }
        }

        BuildMatrices(renderer, bones, game_[0], false);
    }

    void StudioAnimation::BuildMatrices(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        goldsrc::mstudiobone_t* bones,
        Pose& pose,
        bool mirror
    )
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        bool viewmodel = false;

        if (mirror)
        {
            goldsrc::cl_entity_t* model = engine_api_.GetViewModel();
            viewmodel = model != nullptr && model == *view.current_entity;
        }

        for (int index = 0; index < (*view.studio_header)->numbones; ++index)
        {
            BuildBoneMatrix(renderer, index, bones, pose, viewmodel);
        }
    }

    void StudioAnimation::BuildBoneMatrix(
        goldsrc::cstrike::CStudioModelRenderer* renderer,
        int index,
        goldsrc::mstudiobone_t* bones,
        Pose& pose,
        bool mirror
    )
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        ncl_math::Matrix3x4 local;

        ncl_math::QuaternionMatrix(ncl_math::Quaternion::Load(pose.quaternion[index]), local);

        for (int axis = 0; axis < 3; ++axis)
        {
            local[axis][3] = pose.position[index][axis];
        }

        if (bones[index].parent == -1)
        {
            if ((mirror &&
                 *data_.right_hand != nullptr &&
                 (*data_.right_hand)->value > 0 &&
                 studio_api_.IsHardware() &&
                 *data_.shield == 0) ||
                (mirror &&
                 *data_.is_czero == 1 &&
                 studio_api_.IsHardware() &&
                 *data_.shield != 0))
            {
                for (float& value : local[1])
                {
                    value *= -1.0f;
                }
            }

            if (studio_api_.IsHardware())
            {
                ConcatMatrix(**view.rotation_matrix, local, (*view.bone_transform)[index]);
                CopyMatrix((*view.bone_transform)[index], (*view.light_transform)[index]);
            }
            else
            {
                ConcatMatrix(**view.alias_transform, local, (*view.bone_transform)[index]);
                ConcatMatrix(**view.rotation_matrix, local, (*view.light_transform)[index]);
            }

            data_.StudioFxTransform(renderer, *view.current_entity, *view.bone_transform + index);
        }
        else
        {
            ConcatMatrix((*view.bone_transform)[bones[index].parent], local, (*view.bone_transform)[index]);
            ConcatMatrix((*view.light_transform)[bones[index].parent], local, (*view.light_transform)[index]);
        }
    }

    void StudioAnimation::StudioMergeBones(goldsrc::cstrike::CStudioModelRenderer* renderer, goldsrc::model_t* model)
    {
        nitroapi::StudioRendererView view = data_.GetRendererView(renderer);
        goldsrc::cl_entity_t* entity = *view.current_entity;

        if (entity->curstate.sequence < 0 || entity->curstate.sequence >= (*view.studio_header)->numseq)
        {
            entity->curstate.sequence = 0;
        }

        goldsrc::mstudioseqdesc_t* sequence = Sequence(*view.studio_header, entity->curstate.sequence);
        double frame = data_.StudioEstimateFrame.InvokeChained(renderer, sequence);
        goldsrc::mstudioanim_t* animation = data_.StudioGetAnim.InvokeChained(renderer, model, sequence);

        data_.StudioCalcRotations.InvokeChained(
            renderer,
            merge_.position,
            merge_.quaternion,
            sequence,
            animation,
            float(frame)
        );

        goldsrc::mstudiobone_t* bones = Bones(*view.studio_header);

        for (int index = 0; index < (*view.studio_header)->numbones; ++index)
        {
            int cached = 0;

            for (; cached < *view.cached_bones; ++cached)
            {
#ifdef _WIN32
                int comparison = _stricmp(bones[index].name, view.cached_names[cached]);
#else
                int comparison = strcasecmp(bones[index].name, view.cached_names[cached]);
#endif

                if (comparison == 0)
                {
                    CopyMatrix(view.cached_bone_transform[cached], (*view.bone_transform)[index]);
                    CopyMatrix(view.cached_light_transform[cached], (*view.light_transform)[index]);

                    break;
                }
            }

            if (cached >= *view.cached_bones)
            {
                BuildBoneMatrix(renderer, index, bones, merge_, false);
            }
        }
    }

    void StudioAnimation::ConcatMatrix(
        const nitroapi::StudioMatrix_t& left,
        const ncl_math::Matrix3x4& right,
        nitroapi::StudioMatrix_t& output
    )
    {
        ncl_math::Matrix3x4 parent = ncl_math::Matrix3x4::Load(left);

        if (&left == &output)
        {
            ncl_math::ConcatTransforms(parent, right, parent);
            parent.Store(output);

            return;
        }

        ncl_math::Matrix3x4 result;
        ncl_math::ConcatTransforms(parent, right, result);
        result.Store(output);
    }
} // namespace studio
