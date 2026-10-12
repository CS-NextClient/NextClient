#include "engine.h"

#include <cstdint>

#include <glad/glad.h>
#include <ncl_studio/StudioMeshBuilder.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/format/studio.h>

#include "common/cmd.h"
#include "common/cvar.h"
#include "common/sys_dll.h"
#include "console/console.h"
#include "graphics/render/MaterialEdit.h"
#include "graphics/render/MaterialManifests.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/StudioIntercept.h"

namespace
{
    goldsrc::engine_studio_api_t g_Original;
    goldsrc::engine_studio_api_t* g_Patched;

    // The client copies the table into its own renderer, so the entry points we wrote into
    // it outlive any attempt to take them back: restoring the engine's copy does not reach
    // the client's. They therefore have to stay callable for the life of the process, and
    // pass straight through once this module has stopped being ready to serve them.
    bool g_PassThrough;

    goldsrc::studiohdr_t* g_Header;
    goldsrc::model_t* g_Model;
    void* g_SubModel;
    uint32_t g_DrawPointsCalls;
    uint32_t g_ShaderDrawCalls;

    uint32_t g_ModelDrawSerial = 1;

    // The client renderer sets both halves of the pair for an entity, but for a player's
    // attached weapon it sets only the header: Mod_Extradata on the weapon model,
    // StudioSetHeader on what came back, no SetRenderModel. The lookup is therefore the
    // only place the header can still be traced to the model it belongs to.
    goldsrc::model_t* g_ExtradataModel;
    void* g_ExtradataResult;

    void* ModExtradata(goldsrc::model_t* model)
    {
        void* extradata = g_Original.Mod_Extradata(model);

        if (!g_PassThrough)
        {
            g_ExtradataModel = model;
            g_ExtradataResult = extradata;
        }

        return extradata;
    }

    void StudioSetHeader(void* header)
    {
        if (g_PassThrough)
        {
            g_Original.StudioSetHeader(header);
            return;
        }

        g_Header = static_cast<goldsrc::studiohdr_t*>(header);
        g_ModelDrawSerial++;

        // Adopted rather than trusted: a header the client did not just look up leaves the
        // model as SetRenderModel last set it, and the draw path checks the pair anyway.
        if (header != nullptr && header == g_ExtradataResult)
        {
            g_Model = g_ExtradataModel;
        }

        g_Original.StudioSetHeader(header);
    }

    void SetRenderModel(goldsrc::model_t* model)
    {
        if (g_PassThrough)
        {
            g_Original.SetRenderModel(model);
            return;
        }

        g_Model = model;
        g_Original.SetRenderModel(model);
    }

    render::StudioLighting g_Lighting;

    void StudioSetupLighting(goldsrc::alight_t* plight)
    {
        g_Original.StudioSetupLighting(plight);

        if (g_PassThrough)
        {
            return;
        }

        g_Lighting.valid = plight != nullptr && plight->plightvec != nullptr;
        if (!g_Lighting.valid)
        {
            return;
        }

        g_Lighting.ambient = static_cast<float>(plight->ambientlight);
        g_Lighting.shade = static_cast<float>(plight->shadelight);

        for (int i = 0; i < 3; i++)
        {
            g_Lighting.color[i] = plight->color[i];
            g_Lighting.light_vec[i] = plight->plightvec[i];
        }
    }

    void StudioSetupModel(int bodypart, void** ppbodypart, void** ppsubmodel)
    {
        // Which submodel is drawn is the engine's decision, taken from the entity's own
        // body value before this returns, so the material panel's preview cannot be applied
        // where the skin family is. Swapped for the duration of the call and put straight
        // back: the entity's state is the client's, and this borrows it rather than sets it.
        goldsrc::cl_entity_t* entity = !g_PassThrough && g_Original.GetCurrentEntity != nullptr
            ? g_Original.GetCurrentEntity()
            : nullptr;

        int preview = entity != nullptr && entity->model != nullptr
            ? render::MaterialEdit_ShownBody(entity->model->name)
            : -1;

        int saved = entity != nullptr ? entity->curstate.body : 0;

        if (preview >= 0)
        {
            entity->curstate.body = preview;
        }

        g_Original.StudioSetupModel(bodypart, ppbodypart, ppsubmodel);

        if (preview >= 0)
        {
            entity->curstate.body = saved;
        }

        if (g_PassThrough)
        {
            return;
        }

        // Passed on exactly as written: the engine hands out the address of its own
        // submodel pointer, not the pointer, and the draw path decides which it is.
        g_SubModel = ppsubmodel != nullptr ? *ppsubmodel : nullptr;
    }

    double g_FrameSeconds;
    uint32_t g_FrameCalls;
    uint32_t g_FrameShaderCalls;

    bool g_ShaderDrewLast;

    void StudioDrawPoints()
    {
        if (g_PassThrough)
        {
            g_Original.StudioDrawPoints();
            return;
        }

        double start = Sys_FloatTime();

        g_DrawPointsCalls++;
        g_FrameCalls++;

        g_ShaderDrewLast = render::Studio_TryDraw(g_Model, g_Header, g_SubModel);

        if (g_ShaderDrewLast)
        {
            g_ShaderDrawCalls++;
            g_FrameShaderCalls++;
        }
        else
        {
            g_Original.StudioDrawPoints();
        }

        g_FrameSeconds += Sys_FloatTime() - start;
    }

    // The shadow is built from the transformed vertex array that the engine's own
    // submission stage fills as a side effect. When this body part went through the shader
    // path that array still holds whichever model last took the fallback, so the shadow
    // would be this model's triangles over another model's positions. No shadow is the
    // lesser wrong until the shader path produces those vertices itself.
    void GL_StudioDrawShadow()
    {
        if (g_PassThrough)
        {
            g_Original.GL_StudioDrawShadow();
            return;
        }

        if (g_ShaderDrewLast)
        {
            // The engine's own shadow stage opens by restoring the depth mask, and it is the
            // only place in the studio loop that does; skipping the stage without this leaves
            // a translucent entity's disabled mask on for everything drawn after it.
            glDepthMask(GL_TRUE);
            return;
        }

        g_Original.GL_StudioDrawShadow();
    }

    // Mod_LoadStudioModel keeps only the part of the file that precedes the texture
    // pixels when the textures are embedded, so the cached buffer is shorter than the
    // length the header declares. Geometry lives before that point either way.
    size_t CachedModelSize(const goldsrc::studiohdr_t* header)
    {
        return header->textureindex != 0
            ? static_cast<size_t>(header->texturedataindex)
            : static_cast<size_t>(header->length);
    }

    void StudioInfo_f()
    {
        if (g_Header == nullptr)
        {
            Con_Printf("r_studio_info: no studio model has been drawn yet\n");
            return;
        }

        ncl_utils::ResultT<ncl_studio::StudioMesh> mesh = ncl_studio::BuildStudioMesh(g_Header, CachedModelSize(g_Header));

        if (mesh.has_error())
        {
            Con_Printf("r_studio_info: %s: %s\n", g_Header->name, mesh.error_str().c_str());
            return;
        }

        Con_Printf(
            "r_studio_info: %s (%s)\n",
            g_Header->name,
            g_Model != nullptr ? g_Model->name : "<no model>"
        );
        Con_Printf(
            "  %u body parts, %u submodels, %u meshes, %u vertices, %u indices\n",
            static_cast<unsigned>(mesh->bodyparts.size()),
            static_cast<unsigned>(mesh->submodels.size()),
            static_cast<unsigned>(mesh->meshes.size()),
            static_cast<unsigned>(mesh->vertices.size()),
            static_cast<unsigned>(mesh->indices.size())
        );
        Con_Printf(
            "  %u submission calls observed, %u of them drawn by the shader path\n",
            g_DrawPointsCalls,
            g_ShaderDrawCalls
        );
        Con_Printf("  last fallback reason: %s\n", render::Studio_LastRejection());
    }
}

void render::Studio_PatchApi(goldsrc::engine_studio_api_t* api)
{
    // The table is handed out again on a level change; comparing against our own entry
    // point both detects that case and keeps a second patch from capturing itself as
    // the original.
    if (api == nullptr || api->StudioDrawPoints == StudioDrawPoints)
    {
        return;
    }

    g_Original = *api;
    g_Patched = api;

    api->Mod_Extradata = ModExtradata;
    api->StudioSetHeader = StudioSetHeader;
    api->SetRenderModel = SetRenderModel;
    api->StudioSetupModel = StudioSetupModel;
    api->StudioSetupLighting = StudioSetupLighting;
    api->StudioDrawPoints = StudioDrawPoints;
    api->GL_StudioDrawShadow = GL_StudioDrawShadow;
}

const render::StudioLighting& render::Studio_Lighting()
{
    return g_Lighting;
}

uint32_t render::Studio_ModelDrawSerial()
{
    return g_ModelDrawSerial;
}

render::StudioFrameStats render::Studio_TakeFrameStats()
{
    StudioFrameStats stats;
    stats.submission_ms = static_cast<float>(g_FrameSeconds * 1000.0);
    stats.calls = g_FrameCalls;
    stats.shader_calls = g_FrameShaderCalls;

    g_FrameSeconds = 0.0;
    g_FrameCalls = 0;
    g_FrameShaderCalls = 0;

    return stats;
}

const goldsrc::engine_studio_api_t* render::Studio_OriginalApi()
{
    return g_Original.StudioDrawPoints != nullptr ? &g_Original : nullptr;
}

void render::Studio_Shutdown()
{
    // Both halves are needed and neither is sufficient. Restoring the table serves whoever
    // reads it after this point; the flag serves everyone already holding a copy of it,
    // which is what the client took at HUD_GetStudioModelInterface time.
    g_PassThrough = true;

    if (g_Patched != nullptr && g_Patched->StudioDrawPoints == StudioDrawPoints)
    {
        g_Patched->Mod_Extradata = g_Original.Mod_Extradata;
        g_Patched->StudioSetHeader = g_Original.StudioSetHeader;
        g_Patched->SetRenderModel = g_Original.SetRenderModel;
        g_Patched->StudioSetupModel = g_Original.StudioSetupModel;
        g_Patched->StudioSetupLighting = g_Original.StudioSetupLighting;
        g_Patched->StudioDrawPoints = g_Original.StudioDrawPoints;
        g_Patched->GL_StudioDrawShadow = g_Original.GL_StudioDrawShadow;
    }

    g_Patched = nullptr;

    Studio_ForgetModel();
}

void render::Studio_ForgetModel()
{
    g_Model = nullptr;
    g_Header = nullptr;
    g_SubModel = nullptr;
    g_ExtradataModel = nullptr;
    g_ExtradataResult = nullptr;
}

namespace
{
    // Owned here rather than by the shader manager: what a shipped source is a program of
    // is decided by the uniform table, which only this stage knows, and an edited manifest
    // has to be picked up by the same action that rebuilds the programs.
    void ReloadShaders_f()
    {
        render::Shaders_Reload();
        render::Manifests_Clear();
        Con_Printf("render: shader cache dropped\n");
        render::Studio_WarmPrograms();
    }

    float g_LastShaderReload;

    // Rebuild on a change rather than leave each program to its next draw: the start-up
    // build runs before config.cfg, so a value set there arrives only through this hook,
    // and the same profile can be re-executed mid-session with the value unchanged.
    void OnShaderReloadChanged(goldsrc::cvar_t* cvar)
    {
        if (cvar->value == g_LastShaderReload)
        {
            return;
        }

        g_LastShaderReload = cvar->value;
        ReloadShaders_f();
    }

    cvarhook_t g_ShaderReloadHook = {OnShaderReloadChanged, nullptr, nullptr};
}

void render::Studio_RegisterCommands()
{
    Cmd_AddCommand("r_studio_info", StudioInfo_f);
    Cmd_AddCommand("r_reload_shaders", ReloadShaders_f);

    Cvar_HookVariable(g_RenderCfg.shader_reload->name, &g_ShaderReloadHook);
}
