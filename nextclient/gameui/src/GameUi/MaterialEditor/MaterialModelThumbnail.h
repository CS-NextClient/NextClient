#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <ncl_utils/safe_result.h>

namespace material_editor
{
struct MaterialModelThumbnail
{
    std::vector<uint8_t> rgba;
};

ncl_utils::ResultT<MaterialModelThumbnail> RenderModelThumbnail(
    std::span<const uint8_t> model,
    std::span<const uint8_t> companion
);

} // namespace material_editor
