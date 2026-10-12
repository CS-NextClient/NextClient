#pragma once

#include <goldsrc/cvar.h>

// Single source of truth for the shader pipeline cvar definitions: the engine_mini
// registration and any consumer (settings UI, diagnostics) read this table, so names,
// defaults, ranges and flags cannot drift apart. Same shape as radar_cvars.h.

struct RenderCvarDef
{
    const char* name;
    const char* default_value;
    float min_value;
    float max_value;
    int flags = 0;
};

inline constexpr RenderCvarDef kRenderCvarDefs[] = {
    // 0 off, 1 errors and high severity only, 2 everything the driver reports.
    {"gl_debug_output", "0", 0.0f, 2.0f},
    // Reads shader sources from the game directory instead of the copies embedded in
    // the module, so a program can be edited and rebuilt without restarting. Deliberately
    // not archived: persisted, it would go on quietly preferring whatever was left in
    // the game directory over what the module ships.
    {"r_shader_reload", "0", 0.0f, 1.0f},
    // Phase A exit criterion: draws a shader-drawn full screen quad over the scene.
    {"r_test_quad", "0", 0.0f, 1.0f},
    // Submits studio geometry through the shader path instead of the engine's own,
    // falling back per model whenever the shader path does not handle it. Archived: it is
    // what makes materials exist at all, and losing it on every restart means opening the
    // material panel to an empty list and having to remember why.
    {"r_studio_shader", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
    // Diagnostics overlay: rolling frame time and studio submission time.
    {"r_studio_graph", "0", 0.0f, 1.0f, goldsrc::FCVAR_ARCHIVE},
    // Resolves materials for studio models. Turning it off leaves the shader path drawing
    // what the engine's own path draws, whatever material content happens to be installed,
    // which is what makes an A/B against the engine mean anything.
    {"r_studio_materials", "1", 0.0f, 1.0f},
};

// Lookup by cvar name; a miss returns a def with a null name. In a constexpr context
// pair it with a static_assert(def.name != nullptr) to catch typos at compile time.
constexpr RenderCvarDef RenderCvarDef_Get(const char* name)
{
    for (const RenderCvarDef& def : kRenderCvarDefs)
    {
        const char* a = def.name;
        const char* b = name;

        while (*a && *a == *b)
        {
            a++;
            b++;
        }

        if (*a == *b)
        {
            return def;
        }
    }

    return RenderCvarDef{};
}
