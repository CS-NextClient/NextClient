#include "cstrike_hack.h"
#include "engine.h"

bool g_bCS_CZ_Flags_Initialized;
bool g_bIsCStrike;
bool g_bIsCZero;
bool g_bIsCZeroRitual;
bool g_bIsTerrorStrike;
bool g_bIsTFC;
bool g_bIsHL1;

void SetCStrikeFlags()
{
    char gamedir[MAX_OSPATH];
    g_engfuncs.pfnGetGameDir(gamedir);

    if (g_bCS_CZ_Flags_Initialized)
    {
        return;
    }

    if (!V_stricmp(gamedir, "cstrike") || !V_stricmp(gamedir, "cstrike_beta"))
    {
        g_bIsCStrike = true;
    }
    else if (!V_stricmp(gamedir, "czero"))
    {
        g_bIsCZero = true;
    }
    else if (!V_stricmp(gamedir, "czeror"))
    {
        g_bIsCZeroRitual = true;
    }
    else if (!V_stricmp(gamedir, "terror"))
    {
        g_bIsTerrorStrike = true;
    }
    else if (!V_stricmp(gamedir, "tfc"))
    {
        g_bIsTFC = true;
    }
    else if (!V_stricmp(gamedir, "valve"))
    {
        g_bIsHL1 = true;
    }

    g_bCS_CZ_Flags_Initialized = true;
}
