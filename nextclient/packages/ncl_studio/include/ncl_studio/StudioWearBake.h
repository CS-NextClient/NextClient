#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <vector>

#include <ncl_utils/safe_result.h>

#include "ncl_studio/StudioTextures.h"

namespace ncl_studio
{
    struct StudioWearMap
    {
        int texture_index{};
        int width{};
        int height{};
        std::vector<uint8_t> pixels;
    };

    struct WearBakeOptions
    {
        int size{};
        int skin_family{};
        float scratch_strength{};
        int scratch_width{};
        int scratch_height{};
        // Red supplies scratch thresholds; zero strength does not read the image.
        std::span<const uint8_t> scratch_rgba;
        // Empty selects every texture used by the skin family.
        std::span<const int> texture_indices;
        std::stop_token stop_token;
    };

    ncl_utils::ResultT<std::vector<StudioWearMap>> BakeStudioWearMaps(
        const void* model_data,
        size_t model_size,
        const StudioTextures& textures,
        const WearBakeOptions& options
    );
} // namespace ncl_studio
