#include "commands.h"

#include <string>
#include <unordered_map>

#include "main.h"

namespace
{
    std::unordered_map<std::string, int> g_ToggleNext;

    void Toggle_f()
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

    void RunHoldToggle(bool pressed)
    {
        int argc = gEngfuncs.Cmd_Argc();
        if (argc < 2)
        {
            gEngfuncs.Con_Printf("Usage: +toggle <command> [<held> <released>]\n");
            return;
        }

        const char* value;
        if (argc >= 4)
        {
            value = pressed ? gEngfuncs.Cmd_Argv(2) : gEngfuncs.Cmd_Argv(3);
        }
        else
        {
            value = pressed ? "1" : "0";
        }

        std::string cmd = std::string(gEngfuncs.Cmd_Argv(1)) + " " + value + "\n";
        gEngfuncs.pfnClientCmd(cmd.c_str());
    }

    void PlusToggle_f()
    {
        RunHoldToggle(true);
    }

    void MinusToggle_f()
    {
        RunHoldToggle(false);
    }
} // namespace

void CommandsInit()
{
    gEngfuncs.pfnAddCommand("toggle", Toggle_f);
    gEngfuncs.pfnAddCommand("+toggle", PlusToggle_f);
    gEngfuncs.pfnAddCommand("-toggle", MinusToggle_f);
}
