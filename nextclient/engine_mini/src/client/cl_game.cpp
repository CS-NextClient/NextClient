#include "../engine.h"
#include <optick.h>
#include "spriteapi.h"

HSPRITE_t ghCrosshair;
wrect_t gCrosshairRc;
int gCrosshairR;
int gCrosshairG;
int gCrosshairB;

void SetCrosshair(HSPRITE_t hspr, wrect_t rc, int r, int g, int b)
{
    OPTICK_EVENT();

    p_g_engdstAddrs->pfnSetCrosshair(&hspr, &rc, &r, &g, &b);
    ghCrosshair = hspr;
    gCrosshairRc = rc;
    gCrosshairR = r;
    gCrosshairG = g;
    gCrosshairB = b;
}

void DrawCrosshair(int x, int y)
{
    OPTICK_EVENT();

    if (ghCrosshair)
    {
        SPR_Set(ghCrosshair, gCrosshairR, gCrosshairG, gCrosshairB);
        SPR_DrawHoles(0, x - (gCrosshairRc.right - gCrosshairRc.left) / 2, y - (gCrosshairRc.bottom - gCrosshairRc.top) / 2, &gCrosshairRc);
    }
}
