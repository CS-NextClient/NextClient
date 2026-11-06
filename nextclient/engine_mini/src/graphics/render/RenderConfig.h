#pragma once

#include <goldsrc/cvar.h>

namespace render
{
    struct RenderConfig
    {
        goldsrc::cvar_t* debug_output;
        goldsrc::cvar_t* test_quad;
        goldsrc::cvar_t* studio_shader;
        goldsrc::cvar_t* studio_materials;
        goldsrc::cvar_t* studio_queue;
    };

    extern RenderConfig g_RenderCfg;

    void Config_Init();
} // namespace render
