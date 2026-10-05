#include "hlsdk.h"
#include "main.h"
#include "commands.h"

#include <string>
#include <unordered_map>

static std::unordered_map<std::string, int> g_ToggleNext;

static void Toggle_f()
{
    int argc = gEngfuncs.Cmd_Argc();
    if (argc < 4)
    {
        gEngfuncs.Con_Printf("Usage: toggle <command> <value1> <value2> [value3...]\n");
        return;
    }

    std::string key;
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
        {
            key += ' ';
        }
        key += gEngfuncs.Cmd_Argv(i);
    }

    int count = argc - 2;
    int& next = g_ToggleNext[key];
    const char* value = gEngfuncs.Cmd_Argv(2 + next);

    std::string cmd = std::string(gEngfuncs.Cmd_Argv(1)) + " " + value + "\n";
    gEngfuncs.pfnClientCmd(cmd.c_str());

    next = (next + 1) % count;
}

void CommandsInit()
{
    gEngfuncs.pfnAddCommand("toggle", Toggle_f);
}
