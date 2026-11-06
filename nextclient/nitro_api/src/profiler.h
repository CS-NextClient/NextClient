#pragma once

#include <string>

#include <magic_enum/magic_enum.hpp>
#include <ncl_perf/profiler.h>
#include <nitroapi/hook/StaticHookId.h>

namespace nitroapi::profiling
{
    template <StaticHookId HookId, bool Original>
    const char* HookName()
    {
        static const std::string name = std::string(Original ? "Original " : "Hook ") +
            std::string(magic_enum::enum_name(HookId));

        return name.c_str();
    }
} // namespace nitroapi::profiling
