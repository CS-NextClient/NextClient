#include "engine.h"

#include <common/cvar.h>
#include <next_engine_mini/render_cvars.h>

#include "graphics/render/RenderConfig.h"
#include "graphics/render/RenderDebug.h"

render::RenderConfig render::g_RenderCfg;

namespace
{
    // A name missing from the shared table yields a null name here and fails the build,
    // instead of silently registering a wrong default at runtime.
    constexpr RenderCvarDef kDebugOutputDef = RenderCvarDef_Get("gl_debug_output");
    static_assert(kDebugOutputDef.name != nullptr);

    constexpr RenderCvarDef kShaderReloadDef = RenderCvarDef_Get("r_shader_reload");
    static_assert(kShaderReloadDef.name != nullptr);

    constexpr RenderCvarDef kTestQuadDef = RenderCvarDef_Get("r_test_quad");
    static_assert(kTestQuadDef.name != nullptr);

    constexpr RenderCvarDef kStudioShaderDef = RenderCvarDef_Get("r_studio_shader");
    static_assert(kStudioShaderDef.name != nullptr);

    // Flags come from the shared table with the name and the default, so a cvar that has to
    // survive a restart cannot be given the flag in one place and registered without it in
    // another.
    cvar_t g_DebugOutput = {kDebugOutputDef.name, const_cast<char*>(kDebugOutputDef.default_value), kDebugOutputDef.flags};
    cvar_t g_ShaderReload = {kShaderReloadDef.name, const_cast<char*>(kShaderReloadDef.default_value), kShaderReloadDef.flags};
    cvar_t g_TestQuad = {kTestQuadDef.name, const_cast<char*>(kTestQuadDef.default_value), kTestQuadDef.flags};
    constexpr RenderCvarDef kStudioGraphDef = RenderCvarDef_Get("r_studio_graph");
    static_assert(kStudioGraphDef.name != nullptr);

    constexpr RenderCvarDef kStudioMaterialsDef = RenderCvarDef_Get("r_studio_materials");
    static_assert(kStudioMaterialsDef.name != nullptr);

    cvar_t g_StudioShader = {kStudioShaderDef.name, const_cast<char*>(kStudioShaderDef.default_value), kStudioShaderDef.flags};
    cvar_t g_StudioMaterials = {kStudioMaterialsDef.name, const_cast<char*>(kStudioMaterialsDef.default_value), kStudioMaterialsDef.flags};
    cvar_t g_StudioGraph = {kStudioGraphDef.name, const_cast<char*>(kStudioGraphDef.default_value), kStudioGraphDef.flags};

    void OnDebugOutputChanged(cvar_t* cvar)
    {
        render::Debug_Apply();
    }

    cvarhook_t g_DebugOutputHook = {OnDebugOutputChanged, nullptr, nullptr};
}

void render::Config_Init()
{
    Cvar_RegisterVariable(&g_DebugOutput);
    Cvar_RegisterVariable(&g_ShaderReload);
    Cvar_RegisterVariable(&g_TestQuad);
    Cvar_RegisterVariable(&g_StudioShader);
    Cvar_RegisterVariable(&g_StudioGraph);
    Cvar_RegisterVariable(&g_StudioMaterials);

    Cvar_HookVariable(g_DebugOutput.name, &g_DebugOutputHook);

    g_RenderCfg.debug_output = &g_DebugOutput;
    g_RenderCfg.shader_reload = &g_ShaderReload;
    g_RenderCfg.test_quad = &g_TestQuad;
    g_RenderCfg.studio_shader = &g_StudioShader;
    g_RenderCfg.studio_graph = &g_StudioGraph;
    g_RenderCfg.studio_materials = &g_StudioMaterials;
}
