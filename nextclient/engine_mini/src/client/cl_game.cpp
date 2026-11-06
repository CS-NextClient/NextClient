#include "../engine.h"
#include <ncl_perf/profiler.h>
#include <goldsrc/client_api.h>
#include <goldsrc/types.h>
#include "spriteapi.h"


goldsrc::HSPRITE_t ghCrosshair;
goldsrc::wrect_t gCrosshairRc;
int gCrosshairR;
int gCrosshairG;
int gCrosshairB;

void SetCrosshair(goldsrc::HSPRITE_t hspr, goldsrc::wrect_t rc, int r, int g, int b)
{
    NCL_PROFILE_SCOPE();

    p_g_engdstAddrs->pfnSetCrosshair(&hspr, &rc, &r, &g, &b);
    ghCrosshair = hspr;
    gCrosshairRc = rc;
    gCrosshairR = r;
    gCrosshairG = g;
    gCrosshairB = b;
}

void DrawCrosshair(int x, int y)
{
    NCL_PROFILE_SCOPE();


    if (ghCrosshair)
    {
        SPR_Set(ghCrosshair, gCrosshairR, gCrosshairG, gCrosshairB);
        SPR_DrawHoles(0, x - (gCrosshairRc.right - gCrosshairRc.left) / 2, y - (gCrosshairRc.bottom - gCrosshairRc.top) / 2, &gCrosshairRc);
    }
}
