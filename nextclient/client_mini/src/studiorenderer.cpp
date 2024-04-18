#include "main.h"
#include "fov.h"
#include "inspect.h"

int	StudioDrawModel(int flags)
{
    cl_entity_t *entity = IEngineStudio.GetCurrentEntity();

    if (entity != IEngineStudio.GetViewEntity())
        return g_OriginalStudio.StudioDrawModel(flags);

    InspectThink();

    return g_OriginalStudio.StudioDrawModel(flags);;
}
