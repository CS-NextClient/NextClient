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
#include <ctime>

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

static int64_t g_StartTime = 0;

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
            { "assets", assets },
            { "timestamps", { { "start", g_StartTime } } }
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
        { "assets", assets },
        { "timestamps", { { "start", g_StartTime } } }
    };
}

static DiscordIpc g_DiscordIpc;
static const char* const kHandshake = R"({"v":1,"client_id":"1538460768503070771"})";

void DiscordPresence_Init()
{
    g_StartTime = static_cast<int64_t>(time(nullptr));
}

void DiscordPresence_Shutdown()
{
    g_DiscordIpc.Close();
}

static bool g_DiscordReady = false;

static double g_NextUpdateTime = 0;
static double g_NextConnectTime = 0;
static double g_NextActivityTime = 0;

static std::string g_LastActivity;
static int g_Nonce = 0;

void DiscordPresence_Frame()
{
    if (*realtime < g_NextUpdateTime)
        return;
    
    g_NextUpdateTime = *realtime + 1.0;
    
    if (!g_DiscordIpc.is_open())
    {
        g_LastActivity.clear();
        g_DiscordReady = false;

        if (*realtime < g_NextConnectTime)
            return;
        
        g_NextConnectTime = *realtime + 15.0;

        if (!g_DiscordIpc.Open())
            return;

        if (!g_DiscordIpc.Write(DiscordOpcode::Handshake, kHandshake))
            return;
    }

    DiscordOpcode opcode;
    std::string message;

    while (g_DiscordIpc.Poll(opcode, message))
    {
        if (opcode == DiscordOpcode::Frame && !g_DiscordReady)
        {
            g_DiscordReady = true;
            Con_Printf("Discord: ready!\n");
        }
        else if (opcode == DiscordOpcode::Ping)
        {
            if (!g_DiscordIpc.Write(DiscordOpcode::Pong, message))
                Con_Printf("Discord: pong failed\n");
        }
        else if (opcode == DiscordOpcode::Close)
        {
            Con_Printf("Discord: close %s\n", message.c_str());
            g_DiscordIpc.Close();
        }
    }

    if (!g_DiscordIpc.is_open() || !g_DiscordReady)
        return;

    tao::json::value activity = BuildActivity();
    std::string activity_str = tao::json::to_string(activity);

    if (activity_str == g_LastActivity)
        return;

    if (*realtime < g_NextActivityTime)
        return;

    g_NextActivityTime = *realtime + 5.0;
    
    tao::json::value command = {
        { "cmd", "SET_ACTIVITY" },
        { "nonce", std::to_string(++g_Nonce) },
        { "args", {
            { "pid", GetCurrentPid() },
            { "activity", activity }
        }}
    };

    std::string payload = tao::json::to_string(command);
    if (!g_DiscordIpc.Write(DiscordOpcode::Frame, payload))
    {
        Con_Printf("Discord: frame failed\n");
        return;
    }

    g_LastActivity = activity_str;
}