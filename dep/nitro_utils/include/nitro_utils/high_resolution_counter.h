#pragma once

#include <cstdint>

namespace nitro_utils
{
#ifdef _WIN32
    uint64_t HRCount();
    uint32_t GetElapsedMs(uint64_t end, uint64_t start);
#endif
}