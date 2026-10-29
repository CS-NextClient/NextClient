#include "graphics/render/RenderConfig.h"

#include <next_engine_mini/render_cvars.h>

#include "engine.h"
#include "common/cvar.h"
#include "graphics/render/render_debug.h"

namespace render
{
    RenderConfig g_RenderCfg;

    namespace
    {
        // A name missing from the shared table yields a null name here and fails the build,
        // instead of silently registering a wrong default at runtime.
        constexpr cvars::CvarDefinition kDebugOutputDef = render::FindCvar("gl_debug_output");
        static_assert(kDebugOutputDef.name != nullptr);

        constexpr cvars::CvarDefinition kTestQuadDef = render::FindCvar("r_test_quad");
        static_assert(kTestQuadDef.name != nullptr);

        constexpr cvars::CvarDefinition kStudioShaderDef = render::FindCvar("r_studio_shader");
        static_assert(kStudioShaderDef.name != nullptr);

        // Flags come from the shared table with the name and the default, so a cvar that has to
        // survive a restart cannot be given the flag in one place and registered without it in
        // another.
        goldsrc::cvar_t g_DebugOutput = {kDebugOutputDef.name, const_cast<char*>(kDebugOutputDef.default_value), kDebugOutputDef.flags};
        goldsrc::cvar_t g_TestQuad = {kTestQuadDef.name, const_cast<char*>(kTestQuadDef.default_value), kTestQuadDef.flags};

        constexpr cvars::CvarDefinition kStudioMaterialsDef = render::FindCvar("r_studio_materials");
        static_assert(kStudioMaterialsDef.name != nullptr);

        constexpr cvars::CvarDefinition kStudioQueueDef = render::FindCvar("r_studio_queue");
        static_assert(kStudioQueueDef.name != nullptr);

        goldsrc::cvar_t g_StudioQueue = {kStudioQueueDef.name, const_cast<char*>(kStudioQueueDef.default_value), kStudioQueueDef.flags};

        goldsrc::cvar_t g_StudioShader = {kStudioShaderDef.name, const_cast<char*>(kStudioShaderDef.default_value), kStudioShaderDef.flags};
        goldsrc::cvar_t g_StudioMaterials = {
            kStudioMaterialsDef.name,
            const_cast<char*>(kStudioMaterialsDef.default_value),
            kStudioMaterialsDef.flags
        };

        void OnDebugOutputChanged(goldsrc::cvar_t* cvar)
        {
            Debug_Apply();
        }

        cvarhook_t g_DebugOutputHook = {OnDebugOutputChanged, nullptr, nullptr};
    } // namespace

    void Config_Init()
    {
        Cvar_RegisterVariable(&g_DebugOutput);
        Cvar_RegisterVariable(&g_TestQuad);
        Cvar_RegisterVariable(&g_StudioShader);
        Cvar_RegisterVariable(&g_StudioMaterials);
        Cvar_RegisterVariable(&g_StudioQueue);

        Cvar_HookVariable(g_DebugOutput.name, &g_DebugOutputHook);

        g_RenderCfg.debug_output = &g_DebugOutput;
        g_RenderCfg.test_quad = &g_TestQuad;
        g_RenderCfg.studio_shader = &g_StudioShader;
        g_RenderCfg.studio_materials = &g_StudioMaterials;
        g_RenderCfg.studio_queue = &g_StudioQueue;
    }
} // namespace render
