#include "hlsdk.h"
#include "main.h"
#include "commands.h"

static void BindToggle_f()
{
    if (gEngfuncs.Cmd_Argc() < 2)
    {
        gEngfuncs.Con_Printf("Usage: BindToggle <cvar>\n");
        return;
    }

    const char* name = gEngfuncs.Cmd_Argv(1);
    const cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(name);

    if (cvar == nullptr)
    {
        gEngfuncs.Con_Printf("BindToggle: unknown cvar \"%s\"\n", name);
        return;
    }

    gEngfuncs.Cvar_SetValue(name, cvar->value != 0.0f ? 0.0f : 1.0f);
}

void CommandsInit()
{
    gEngfuncs.pfnAddCommand("BindToggle", BindToggle_f);
}
