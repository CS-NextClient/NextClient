#include "graphics/preview/preview.h"

#include "engine.h"

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/render/RendererState.h"
#include "common/model.h"
#include "graphics/preview/preview_model.h"
#include "graphics/render/effects/studio_effects.h"
#include "graphics/render/render_device.h"
#include "graphics/render/StudioDraw.h"

namespace
{
    IBaseInterface* CreateScenePreview()
    {
        return &render::GetRendererState().preview;
    }
} // namespace

EXPOSE_INTERFACE_FN(CreateScenePreview, ScenePreviewInterface, SCENE_PREVIEW_INTERFACE_VERSION);

void Preview_Init()
{
    render::RendererState& state = render::GetRendererState();
    state.preview_engine = {
        pStudioAPI,
        p_currententity,
        p_r_blend,
        mod_known,
        &mod_numknown,
        Mod_ForName,
        Mod_Extradata,
        cls,
        gEngfuncs.pfnGetScreenInfo,
        gEngfuncs.Con_Printf
    };
    state.preview_assets.BindFiles(gEngfuncs.COM_LoadFile, gEngfuncs.COM_FreeFile);
    gEngfuncs.pfnAddCommand("ncl_preview_scene_reload", [] { render::GetRendererState().preview.ReloadAll(); });
    gEngfuncs.pfnAddCommand("r_preview_state", Preview_DumpState);
}

void Preview_Shutdown()
{
    render::GetRendererState().preview.FreeAll();
}

void Preview_UnbindEngine()
{
    render::RendererState& state = render::GetRendererState();
    state.preview_renderer.RestoreStudioApi();
    state.preview_engine = {};
    state.preview_assets.BindFiles(nullptr, nullptr);
}

void Preview_DumpState()
{
    const render::StudioEffectsStats effects = render::StudioEffects_LastStats();
    gEngfuncs.Con_Printf(
        "material effects: last_entities=%u, last_packets=%u, mask_pixels=%llu, reused_targets=%u, depth_format=%u, samples=%d\n",
        effects.entities,
        effects.packets,
        static_cast<unsigned long long>(effects.mask_pixels),
        effects.reused_targets,
        effects.depth_format,
        effects.samples
    );

    gEngfuncs.Con_Printf(
        "preview renderer: studio=%s, device_generation=%u, world=%s, loaded_entries=%d/128, studio_result=%s\nlast_load_error=%s\n",
        pStudioAPI != nullptr && pStudioAPI->StudioDrawModel != nullptr ? "ready" : "unavailable",
        render::Device_Generation(),
        cl != nullptr && cl->worldmodel != nullptr ? "loaded" : "none",
        PreviewModel_LoadedEntries(),
        render::Studio_LastRejection(),
        PreviewModel_LastLoadError()
    );
}
