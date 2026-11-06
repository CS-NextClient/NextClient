#include "ncl_studio/bone_animation.h"

#include <cstdlib>

#include <ncl_math/vec3.h>

namespace ncl_studio
{
    namespace
    {
        constexpr float kControllerLoopScale = 1.40625f;
        constexpr float kControllerByteScale = 1.0f / 255.0f;

        struct AnimationRun
        {
            const goldsrc::mstudioanimvalue_t* values{};
            int frame{};
        };

        AnimationRun SelectRun(const goldsrc::mstudioanim_t& animation, int channel, int frame)
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&animation);
            const goldsrc::mstudioanimvalue_t* values =
                reinterpret_cast<const goldsrc::mstudioanimvalue_t*>(bytes + animation.offset[channel]);

            if (values->num.total < values->num.valid)
            {
                frame = 0;
            }

            while (values->num.total <= frame)
            {
                frame -= values->num.total;
                values += values->num.valid + 1;

                if (values->num.total < values->num.valid)
                {
                    frame = 0;
                }
            }

            return {values, frame};
        }
    } // namespace

    void CalcBoneAdj(
        const goldsrc::studiohdr_t& header,
        float interpolant,
        float* adjustment,
        const uint8_t* current,
        const uint8_t* previous,
        uint8_t mouth
    )
    {
        const float inverse = 1.0f - interpolant;

        for (int row = 0; row < header.numbonecontrollers; ++row)
        {
            const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
            const goldsrc::mstudiobonecontroller_t* controllers =
                reinterpret_cast<const goldsrc::mstudiobonecontroller_t*>(header_bytes + header.bonecontrollerindex);
            const goldsrc::mstudiobonecontroller_t& controller = controllers[row];
            const int index = controller.index;
            float value = 0;

            if (index <= 3)
            {
                if ((controller.type & goldsrc::STUDIO_RLOOP) != 0)
                {
                    if (std::abs(int(current[index]) - int(previous[index])) > 128)
                    {
                        // The wrap threshold uses the input index, but the wrapped bytes use the controller row.
                        const int first = (current[row] + 128) % 256;
                        const int second = (previous[row] + 128) % 256;
                        value = (first * interpolant + second * inverse - 128.0f) * kControllerLoopScale + controller.start;
                    }
                    else
                    {
                        value = (current[index] * interpolant + previous[index] * inverse) * kControllerLoopScale + controller.start;
                    }
                }
                else
                {
                    const float blended = current[index] * interpolant + previous[index] * inverse;
                    float factor = blended * kControllerByteScale;

                    if (factor < 0)
                    {
                        factor = 0;
                    }

                    if (factor > 1)
                    {
                        factor = 1;
                    }

                    value = controller.start * (1.0f - factor) + controller.end * factor;
                }
            }
            else
            {
                float factor = mouth * (1.0f / 64.0f);

                if (factor > 1)
                {
                    factor = 1;
                }

                value = controller.start * (1.0f - factor) + controller.end * factor;
            }

            switch (controller.type & goldsrc::STUDIO_TYPES)
            {
                case goldsrc::STUDIO_X:
                case goldsrc::STUDIO_Y:
                case goldsrc::STUDIO_Z:
                    {
                        adjustment[row] = value;
                        break;
                    }

                case goldsrc::STUDIO_XR:
                case goldsrc::STUDIO_YR:
                case goldsrc::STUDIO_ZR:
                    {
                        adjustment[row] = value * ncl_math::kDeg2Rad;
                        break;
                    }
            }
        }
    }

    void CalcBoneQuaternion(
        int frame,
        float fraction,
        const goldsrc::mstudiobone_t& bone,
        const goldsrc::mstudioanim_t& animation,
        const float* adjustment,
        ncl_math::Quaternion& quaternion
    )
    {
        ncl_math::Vector3 first_angles{};
        ncl_math::Vector3 second_angles{};

        for (int axis = 0; axis < 3; ++axis)
        {
            const int channel = axis + 3;
            float first = bone.value[channel];
            float second = first;

            if (animation.offset[channel] != 0)
            {
                const AnimationRun run = SelectRun(animation, channel, frame);
                const int valid = run.values->num.valid;
                const int total = run.values->num.total;
                const int first_index = valid > run.frame ? run.frame + 1 : valid;
                int second_index = first_index;

                if (valid > run.frame + 1)
                {
                    second_index = run.frame + 2;
                }
                else if (total <= run.frame + 1)
                {
                    second_index = valid + 2;
                }

                first += float(run.values[first_index].value) * bone.scale[channel];
                second += float(run.values[second_index].value) * bone.scale[channel];
            }

            const int controller = bone.bonecontroller[channel];

            if (controller != -1)
            {
                first += adjustment[controller];
                second += adjustment[controller];
            }

            first_angles[axis] = first;
            second_angles[axis] = second;
        }

        if (first_angles == second_angles)
        {
            ncl_math::AngleQuaternion(first_angles, quaternion);

            return;
        }

        ncl_math::Quaternion first_quaternion{};
        ncl_math::Quaternion second_quaternion{};

        ncl_math::AngleQuaternion(first_angles, first_quaternion);
        ncl_math::AngleQuaternion(second_angles, second_quaternion);

        ncl_math::QuaternionSlerp(first_quaternion, second_quaternion, fraction, quaternion);
    }

    void CalcBonePosition(
        int frame,
        float fraction,
        const goldsrc::mstudiobone_t& bone,
        const goldsrc::mstudioanim_t& animation,
        const float* adjustment,
        ncl_math::Vector3& position
    )
    {
        const float inverse = 1.0f - fraction;

        for (int axis = 0; axis < 3; ++axis)
        {
            float value = bone.value[axis];
            position[axis] = value;

            if (animation.offset[axis] != 0)
            {
                const AnimationRun run = SelectRun(animation, axis, frame);
                const int valid = run.values->num.valid;
                const int first_index = valid > run.frame ? run.frame + 1 : valid;
                float sample = run.values[first_index].value;

                if (valid > run.frame)
                {
                    if (valid > run.frame + 1)
                    {
                        sample = sample * inverse + run.values[run.frame + 2].value * fraction;
                    }
                }
                else if (run.values->num.total <= run.frame + 1)
                {
                    sample = sample * inverse + run.values[valid + 2].value * fraction;
                }

                value += bone.scale[axis] * sample;
                position[axis] = value;
            }

            value = position[axis];
            const int controller = bone.bonecontroller[axis];

            if (controller != -1 && adjustment != nullptr)
            {
                position[axis] = value + adjustment[controller];
            }
        }
    }
} // namespace ncl_studio
