#pragma once

#include <cstddef>
#include <vector>

#include <ncl_math/vec3.h>
#include <ncl_utils/safe_result.h>

namespace ncl_studio
{
    struct StudioBoneTransform
    {
        ncl_math::Vector3 basis_x{};
        ncl_math::Vector3 basis_y{};
        ncl_math::Vector3 basis_z{};
        ncl_math::Vector3 origin{};

        ncl_math::Vector3 Apply(const ncl_math::Vector3& point) const;
        ncl_math::Vector3 ApplyVector(const ncl_math::Vector3& vector) const;
    };

    // Default bone values, in model space, with parent transforms composed before children.
    ncl_utils::ResultT<std::vector<StudioBoneTransform>> ReadStudioReferencePose(const void* data, size_t size);
} // namespace ncl_studio
