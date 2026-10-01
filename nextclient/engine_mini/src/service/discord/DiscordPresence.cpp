#include "DiscordPresence.h"

#include "engine.h"
#include "console/console.h"
#include "DiscordIpc.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <cvars/cvar_defaults.h>
#include <tao/json.hpp>
#include <cstring>
#include <cstdio>
#include <cctype>
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

static bool IsSafeServerAddress(const char* address)
{
    if (address == nullptr)
        return false;

    size_t length = strlen(address);
    if (length == 0 || length > 63)
        return false;

    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = address[i];

        if (!isalnum(c) && c != '.' && c != ':' && c != '-')
            return false;
    }

    return true;
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

    tao::json::value button = { { "label", "Get NextClient" }, { "url", "https://nextclient.ru/" } };
    tao::json::value buttons = tao::json::value::array({ button });

    if (cls->state != ca_active)
    {
        return {
            { "details", "Counter-Strike 1.6" },
            { "state", "In main menu" },
            { "assets", assets },
            { "buttons", buttons },
            { "timestamps", { { "start", g_StartTime } } }
        };
    }

    char map[64];
    GetMapName(cl->levelname, map, sizeof(map));

    bool is_local = strcmp(cls->servername, "local") == 0;

    tao::json::value activity = {
        { "details", static_cast<const char*>(map) },
        { "state", "On a server" },
        { "party", {
            { "size", tao::json::value::array({ CountPlayers(), cl->maxclients }) }
        }},
        { "assets", assets },
        { "timestamps", { { "start", g_StartTime } } }
    };

    if (!is_local)
    {
        activity["party"]["id"] = std::string("party-") + static_cast<const char*>(cls->servername);
        activity["secrets"] = { { "join", static_cast<const char*>(cls->servername) } };
    }
    else 
    {
        activity["buttons"] = buttons;
    }

    return activity;
}

static DiscordIpc g_DiscordIpc;
static const char* const kHandshake = R"({"v":1,"client_id":"1538460768503070771"})";
static cvar_t* g_DiscordRpcCvar = nullptr;

void DiscordPresence_Init()
{
    g_StartTime = static_cast<int64_t>(time(nullptr));
    g_DiscordRpcCvar = gEngfuncs.pfnRegisterVariable(cvars::kDiscordRpc.name, cvars::kDiscordRpc.value, FCVAR_ARCHIVE);
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
    if (g_DiscordRpcCvar->value == 0)
    {
        g_NextConnectTime = 0;
        g_DiscordIpc.Close();
        return;
    }

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

            tao::json::value subscribe = {
                { "cmd", "SUBSCRIBE" },
                { "evt", "ACTIVITY_JOIN" },
                { "nonce", std::to_string(++g_Nonce) }
            };
            g_DiscordIpc.Write(DiscordOpcode::Frame, tao::json::to_string(subscribe));
        }
        else if (opcode == DiscordOpcode::Frame)
        {
            tao::json::value json;

            try
            {
                json = tao::json::from_string(message);
            }
            catch(const std::exception& e)
            {
                Con_Printf("Discord: bad message: %s\n", e.what());
                continue;
            }

            const tao::json::value* evt = json.find("evt");
            if (evt == nullptr || !evt->is_string())
                continue;
            
            if (evt->get_string() == "ERROR")
            {
                Con_Printf("Discord: error %s\n", message.c_str());
            }
            else if (evt->get_string() == "ACTIVITY_JOIN")
            {
                const tao::json::value* data = json.find("data");
                if (data == nullptr || !data->is_object())
                    continue;
                
                const tao::json::value* secret = data->find("secret");
                if (secret == nullptr || !secret->is_string())
                    continue;
                
                const std::string& address = secret->get_string();

                if (!IsSafeServerAddress(address.c_str()))
                {
                    Con_Printf("Discord: rejected join address\n");
                    continue;
                }

                char command[96];
                snprintf(command, sizeof(command), "connect %s\n", address.c_str());
                gEngfuncs.pfnClientCmd(command);

                Con_Printf("Discord: join %s\n", address.c_str());
            }
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