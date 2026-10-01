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
#include <cstring>
#include <cstdio>

static int GetCurrentPid()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void GetMapName(const char* levelname, char* out, size_t out_size)
{
    const char* slash = strrchr(levelname, '/');
    const char* start = slash ? slash + 1 : levelname;

    snprintf(out, out_size, "%s", start);

    char* dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

static int CountPlayers()
{
    int players = 0;

    for (int i = 0; i <cl->maxclients; i++)
    {
        if (cl->players[i].name[0] != '\0')
            players++;
    }

    return players;
}

static tao::json::value BuildActivity()
{
    tao::json::value assets = {
        { "large_image", "logo" },
        { "large_text", "NextClient" }
    };

    if (cls->state != ca_active)
    {
        return {
            { "details", "Counter-Strike 1.6" },
            { "state", "In main menu" },
            { "assets", assets }
        };
    }

    char map[64];
    GetMapName(cl->levelname, map, sizeof(map));

    return {
        { "details", static_cast<const char*>(map) },
        { "state", "On a server" },
        { "party", {
            { "size", tao::json::value::array({ CountPlayers(), cl->maxclients }) }
        }},
        { "assets", assets }
    };
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
            { "activity", BuildActivity() }
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
