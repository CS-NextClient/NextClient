#include "graphics/preview/preview.h"

#include "engine.h"

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/ScenePreview.h"

namespace
{
    ScenePreview g_ScenePreview;
} // namespace

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(ScenePreview, ScenePreviewInterface, SCENE_PREVIEW_INTERFACE_VERSION, g_ScenePreview);

void Preview_Init()
{
    gEngfuncs.pfnAddCommand("ncl_preview_scene_reload", [] { g_ScenePreview.ReloadAll(); });
}

void Preview_Shutdown()
{
    g_ScenePreview.FreeAll();
}
