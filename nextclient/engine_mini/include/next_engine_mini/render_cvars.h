#pragma once

#include <cvars/CvarDefinition.h>
#include <goldsrc/cvar.h>

namespace render
{
    inline constexpr cvars::CvarDefinition kCvarDefinitions[] = {
        // 0 off, 1 errors and high severity only, 2 everything the driver reports.
        {"gl_debug_output", "0", 0.0f, 2.0f},
        {"r_test_quad", "0", 0.0f, 1.0f},
        {"r_studio_shader", "1", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
        {"r_studio_materials", "1", 0.0f, 1.0f},
        // Custom studio callbacks issuing raw GL need immediate submission or an explicit barrier.
        {"r_studio_queue", "1", 0.0f, 1.0f},
    };

    constexpr cvars::CvarDefinition FindCvar(std::string_view name)
    {
        return cvars::FindDefinition(kCvarDefinitions, name);
    }
} // namespace render
