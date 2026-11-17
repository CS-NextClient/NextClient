#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <ncl_utils/safe_result.h>

namespace ncl_image
{
    ncl_utils::ResultT<std::vector<uint8_t>> EncodePng(int width, int height, int channels, std::span<const uint8_t> pixels);
}
