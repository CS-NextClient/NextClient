#include "DiscordPresence.h"

#include "engine.h"
#include "console/console.h"
#include "DiscordIpc.h"
#include "DiscordHostname.h"
#include "DiscordValidation.h"
#include "DiscordUrlScheme.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <service/geoip/GeoIpCountryDatabase.h>
#include <cvars/cvar_defaults.h>
#include <tao/json.hpp>
#include <strtools.h>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <ctime>

static int GetCurrentPid()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void GetFileBaseName(const char* path, char* out, size_t out_size)
{
    const char* slash = strrchr(path, '/');
    const char* start = slash ? slash + 1 : path;

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

static std::string Localized(const char* token, const char* english)
{
    const wchar_t* wide = g_pLocalize->Find(token);
    if (wide == nullptr)
        return english;

    std::string utf8;
    utf8.resize(wcslen(wide) * 4 + 1);
    V_UnicodeToUTF8(wide, utf8.data(), static_cast<int>(utf8.size()));
    utf8.resize(strlen(utf8.c_str()));
    return utf8;
}

static int64_t g_StartTime = 0;

static tao::json::value BuildActivity()
{
    tao::json::value assets = {
        { "large_image", "logo" },
        { "large_text", "NextClient" }
    };

    tao::json::value button = { { "label", Localized("#NextClient_Discord_GetNextClient", "Get NextClient") }, { "url", "https://nextclient.ru/" } };
    tao::json::value buttons = tao::json::value::array({ button });

    tao::json::value activity = {
        { "details", "Counter-Strike 1.6" },
        { "assets", assets },
        { "timestamps", { { "start", g_StartTime } } }
    };

    if (cls->state == ca_disconnected)
    {
        activity["state"] = Localized("#NextClient_Discord_MainMenu", "In main menu");
        activity["buttons"] = buttons;
        return activity;
    }

    if (!cls->demoplayback)
    {
        if (cls->state == ca_connecting || cls->state == ca_connected || cls->state == ca_uninitialized)
        {
            activity["state"] = Localized("#NextClient_Discord_Connecting", "Connecting...");
            activity["buttons"] = buttons;
            return activity;
        }

        if (cls->state == ca_active)
        {
            bool is_local = strcmp(cls->servername, "local") == 0;

            if (is_local)
            {
                activity["state"] = Localized("#NextClient_Discord_SinglePlayer", "Single-player");
                activity["buttons"] = buttons;
            }
            else
            {
                const std::string& hostname = DiscordHostname_Get();
                if (!hostname.empty() && IsValidUtf8(hostname))
                {
                    size_t size = GeoIp_GetUtf8PrefixSize(hostname, 128);
                    activity["details"] = hostname.substr(0, size);
                }
                else activity["details"] = Localized("#NextClient_Discord_OnServer", "On a server");

                activity["state"] = Localized("#NextClient_Discord_Multiplayer", "Multiplayer");
                activity["party"]["id"] = std::string("party-") + static_cast<const char*>(cls->servername);
                activity["secrets"] = { { "join", static_cast<const char*>(cls->servername) } };
            }

            activity["party"]["size"] = tao::json::value::array({ CountPlayers(), cl->maxclients });
        }
    }
    else
    {
        activity["buttons"] = buttons;

        if (cls->timedemo)
        {
            activity["state"] = Localized("#NextClient_Discord_Benchmark", "Running a benchmark");
            return activity;
        }

        char map[64];
        GetFileBaseName(cl->levelname, map, sizeof(map));

        activity["details"] = Localized("#NextClient_Discord_WatchingDemo", "Watching a demo");
        activity["state"] = std::string(map) + " (" + std::to_string(CountPlayers()) + "/" + std::to_string(cl->maxclients) + ")";
        activity["type"] = 3;
    }

    return activity;
}

static DiscordIpc g_DiscordIpc;
#define DISCORD_APP_ID "1538460768503070771"

static const char* const kHandshake = R"({"v":1,"client_id":")" DISCORD_APP_ID R"("})";
static cvar_t* g_DiscordRpcCvar = nullptr;

void DiscordPresence_Init()
{
    DiscordHostname_Init();
    DiscordUrlScheme_Register(DISCORD_APP_ID);

    g_StartTime = static_cast<int64_t>(time(nullptr));
    g_DiscordRpcCvar = gEngfuncs.pfnRegisterVariable(cvars::kDiscordRpc.name, cvars::kDiscordRpc.value, FCVAR_ARCHIVE);
}

void DiscordPresence_Shutdown()
{
    DiscordHostname_Shutdown();
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

    DiscordHostname_Update();
    
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