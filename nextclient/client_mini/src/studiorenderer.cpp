#include "main.h"
#include "fov.h"
#include "inspect.h"

int StudioDrawModel(int flags)
{
    cl_entity_t* entity = IEngineStudio.GetCurrentEntity();

    if (entity == IEngineStudio.GetViewEntity())
    {
        // StudioSetRenderamt recalculates r_blend via CL_FxBlend, which returns renderamt regardless of rendermode;
        // calling it for kRenderNormal (renderamt 0) zeroes r_blend and alpha test discards masked meshes entirely
        if (entity->curstate.rendermode != kRenderNormal)
        {
            IEngineStudio.StudioSetRenderamt(entity->curstate.renderamt);
        }

        InspectThink();
    }

    return g_OriginalStudio.StudioDrawModel(flags);
}
