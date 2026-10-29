#include "ncl_studio/StudioReferencePose.h"

#include <cmath>
#include <string>

#include <goldsrc/format/studio.h>

#include "HeaderShape.h"
#include "table_bounds.h"

namespace ncl_studio
{
    namespace
    {
        StudioBoneTransform LocalTransform(const goldsrc::mstudiobone_t& bone)
        {
            float sx = std::sin(bone.value[3]);
            float cx = std::cos(bone.value[3]);
            float sy = std::sin(bone.value[4]);
            float cy = std::cos(bone.value[4]);
            float sz = std::sin(bone.value[5]);
            float cz = std::cos(bone.value[5]);

            // GoldSrc bone angles are radians about X, Y, Z, composed as Rz * Ry * Rx.
            return StudioBoneTransform{
                {cy * cz, cy * sz, -sy},
                {sx * sy * cz - cx * sz, sx * sy * sz + cx * cz, sx * cy},
                {cx * sy * cz + sx * sz, cx * sy * sz - sx * cz, cx * cy},
                {bone.value[0], bone.value[1], bone.value[2]},
            };
        }

        bool BuildBone(
            size_t index,
            const goldsrc::mstudiobone_t* bones,
            std::vector<StudioBoneTransform>& transforms,
            std::vector<uint8_t>& state
        )
        {
            if (state[index] == 2)
            {
                return true;
            }

            if (state[index] == 1)
            {
                return false;
            }

            state[index] = 1;
            int parent = bones[index].parent;
            StudioBoneTransform local = LocalTransform(bones[index]);

            if (parent != -1)
            {
                if (parent < 0 || static_cast<size_t>(parent) >= transforms.size() ||
                    !BuildBone(static_cast<size_t>(parent), bones, transforms, state))
                {
                    return false;
                }

                const StudioBoneTransform& ancestor = transforms[parent];
                local = StudioBoneTransform{
                    ancestor.ApplyVector(local.basis_x),
                    ancestor.ApplyVector(local.basis_y),
                    ancestor.ApplyVector(local.basis_z),
                    ancestor.Apply(local.origin),
                };
            }

            transforms[index] = local;
            state[index] = 2;
            return true;
        }
    } // namespace

    ncl_math::Vector3 StudioBoneTransform::Apply(const ncl_math::Vector3& point) const
    {
        return origin + ApplyVector(point);
    }

    ncl_math::Vector3 StudioBoneTransform::ApplyVector(const ncl_math::Vector3& vector) const
    {
        return basis_x * vector.x + basis_y * vector.y + basis_z * vector.z;
    }

    ncl_utils::ResultT<std::vector<StudioBoneTransform>> ReadStudioReferencePose(const void* data, size_t size)
    {
        if (data == nullptr || size < sizeof(goldsrc::studiohdr_t))
        {
            return ncl_utils::ResultError("buffer is smaller than a studio header");
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        const goldsrc::studiohdr_t* header = reinterpret_cast<const goldsrc::studiohdr_t*>(bytes);
        std::string shape = HeaderShapeError(header);

        if (!shape.empty())
        {
            return ncl_utils::ResultError(std::move(shape));
        }

        if (!TableFits(size, header->boneindex, header->numbones, sizeof(goldsrc::mstudiobone_t)))
        {
            return ncl_utils::ResultError("bone array lies outside the buffer");
        }

        const goldsrc::mstudiobone_t* bones = reinterpret_cast<const goldsrc::mstudiobone_t*>(bytes + header->boneindex);

        for (int index = 0; index < header->numbones; index++)
        {
            for (float value : bones[index].value)
            {
                if (!std::isfinite(value))
                {
                    return ncl_utils::ResultError("bone declares a nonfinite reference value");
                }
            }
        }

        std::vector<StudioBoneTransform> result(header->numbones);
        std::vector<uint8_t> state(header->numbones);

        for (size_t index = 0; index < result.size(); index++)
        {
            if (!BuildBone(index, bones, result, state))
            {
                return ncl_utils::ResultError("bone parent is outside the model or forms a cycle");
            }
        }

        return std::move(result);
    }
} // namespace ncl_studio
