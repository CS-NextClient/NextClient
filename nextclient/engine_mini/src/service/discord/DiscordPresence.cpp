#include "DiscordPresence.h"

#include "engine.h"
#include "console/console.h"
#include "DiscordIpc.h"

static DiscordIpc g_DiscordIpc;

static void DiscordTest_f()
{
    if (!g_DiscordIpc.Open())
    {
        Con_Printf("Discord: client not found\n");
        return;
    }

    const char* handshake = R"({"v":1,"client_id":"1538460768503070771"})";
    
    if (!g_DiscordIpc.Write(DiscordOpcode::Handshake, handshake))
    {
        Con_Printf("Discord: handshake failed\n");
        return;
    }

    DiscordOpcode opcode;
    std::string reply;

    if (!g_DiscordIpc.Read(opcode, reply))
    {
        Con_Printf("Discord: no reply\n");
        return;
    }

    Con_Printf("Discord: %s\n", reply.c_str());
}

void DiscordPresence_Init()
{
    gEngfuncs.pfnAddCommand("discord_test", DiscordTest_f);
}

void DiscordPresence_Shutdown()
{
    g_DiscordIpc.Close();
}
