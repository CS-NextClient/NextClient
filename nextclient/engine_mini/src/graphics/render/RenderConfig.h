#pragma once

#include <goldsrc/cvar.h>

namespace render
{
    struct RenderConfig
    {
        goldsrc::cvar_t* debug_output;
        goldsrc::cvar_t* shader_reload;
        goldsrc::cvar_t* test_quad;
        goldsrc::cvar_t* studio_shader;
        goldsrc::cvar_t* studio_graph;
        goldsrc::cvar_t* studio_materials;
    };

    extern RenderConfig g_RenderCfg;

    void Config_Init();
}
