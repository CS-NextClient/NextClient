#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <ncl_math/vec3.h>
#include <ncl_utils/safe_result.h>

namespace ncl_studio
{
    struct StudioSequence
    {
        std::string name;
        float fps{};
        int32_t frame_count{};
    };

    struct StudioBodyPart
    {
        int32_t base{};
        std::vector<std::string> submodels;
    };

    struct StudioMetadata
    {
        ncl_math::Vector3 minimum{};
        ncl_math::Vector3 maximum{};
        std::vector<StudioSequence> sequences;
        std::vector<StudioBodyPart> bodyparts;
    };

    ncl_utils::ResultT<StudioMetadata> ReadStudioMetadata(const void* data, size_t size);
} // namespace ncl_studio
