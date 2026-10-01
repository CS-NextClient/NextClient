#include "DiscordPresence.h"

#include "engine.h"
#include "console/console.h"
#include "DiscordIpc.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <tao/json.hpp>

static int GetCurrentPid()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

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

    tao::json::value activity = {
        { "cmd", "SET_ACTIVITY"},
        { "nonce", "1" },
        { "args", {
            { "pid", GetCurrentPid() },
            { "activity", {
                { "details", "Counter-Strike 1.6" },
                { "state", "In main menu" },
                { "assets", {
                    { "large_image", "logo" },
                    { "large_text", "NextClient" }
                }}
            }}
        }}
    };

    std::string payload = tao::json::to_string(activity);
    Con_Printf("Discord: %s\n", payload.c_str());

    if (!g_DiscordIpc.Write(DiscordOpcode::Frame, payload))
    {
        Con_Printf("Discord: frame failed\n");
        return;
    }

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
