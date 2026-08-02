#pragma once

#include <common/cvar.h>

namespace render
{
    struct RenderConfig
    {
        cvar_t* debug_output;
        cvar_t* shader_reload;
        cvar_t* test_quad;
        cvar_t* studio_shader;
        cvar_t* studio_graph;
        cvar_t* studio_materials;
    };

    extern RenderConfig g_RenderCfg;

    void Config_Init();
}
