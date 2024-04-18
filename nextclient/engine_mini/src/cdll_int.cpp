#include "engine.h"
#include <optick.h>

void ClientDLL_DrawTransparentTriangles()
{
    OPTICK_EVENT();

    eng()->ClientDLL_DrawTransparentTriangles.InvokeChained();
}

void ClientDLL_DrawNormalTriangles()
{
    OPTICK_EVENT();

    eng()->ClientDLL_DrawNormalTriangles.InvokeChained();
}