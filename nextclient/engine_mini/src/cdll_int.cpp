#include "engine.h"
#include <ncl_perf/profiler.h>

void ClientDLL_DrawTransparentTriangles()
{
    NCL_PROFILE_SCOPE();

    eng()->ClientDLL_DrawTransparentTriangles.InvokeChained();
}

void ClientDLL_DrawNormalTriangles()
{
    NCL_PROFILE_SCOPE();

    eng()->ClientDLL_DrawNormalTriangles.InvokeChained();
}