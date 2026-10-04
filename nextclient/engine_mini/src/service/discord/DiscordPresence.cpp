#include "DiscordPresence.h"

#include "engine.h"
#include "console/console.h"
#include "DiscordHostname.h"
#include "DiscordUrlScheme.h"
#include "DiscordWorker.h"

#include <service/geoip/GeoIpCountryDatabase.h>
#include <cvars/cvar_defaults.h>
#include <tao/json.hpp>
#include <strtools.h>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <ctime>

namespace
{
    constexpr const char* kDiscordAppId = "1538460768503070771";

    cvar_t* g_DiscordRpcCvar = nullptr;
    cvar_t* g_DiscordRpcServerCvar = nullptr;
    cvar_t* g_DiscordRpcJoinCvar = nullptr;

    DiscordWorker g_DiscordWorker(kDiscordAppId);
    std::string g_LastActivity;
    int64_t g_StartTime = 0;

    double g_NextUpdateTime = 0;

    void GetFileBaseName(const char* path, char* out, size_t out_size)
    {
        const char* slash = strrchr(path, '/');
        const char* start = slash ? slash + 1 : path;

        snprintf(out, out_size, "%s", start);

        char* dot = strrchr(out, '.');
        if (dot)
        {
            *dot = '\0';
        }
    }

    int CountPlayers()
    {
        int players = 0;

        for (int i = 0; i < cl->maxclients; i++)
        {
            if (cl->players[i].name[0] != '\0')
            {
                players++;
            }
        }

        return players;
    }

    std::string Localized(const char* token, const char* english)
    {
        const wchar_t* wide = g_pLocalize->Find(token);
        if (wide == nullptr)
        {
            return english;
        }

        std::string utf8;
        utf8.resize(wcslen(wide) * 4 + 1);
        V_UnicodeToUTF8(wide, utf8.data(), static_cast<int>(utf8.size()));
        utf8.resize(strlen(utf8.c_str()));
        return utf8;
    }

    // What Discord gets when it keeps rejecting the full activity: nothing taken from the server
    tao::json::value BuildFallbackActivity()
    {
        return {
            {"details", "Counter-Strike 1.6"},
            {"assets", {{"large_image", "logo"}, {"large_text", "NextClient"}}},
            {"timestamps", {{"start", g_StartTime}}}
        };
    }

    tao::json::value BuildActivity()
    {
        tao::json::value assets = {{"large_image", "logo"}, {"large_text", "NextClient"}};

        tao::json::value button = {
            {"label", Localized("#NextClient_Discord_GetNextClient", "Get NextClient")}, {"url", "https://nextclient.ru/"}
        };
        tao::json::value buttons = tao::json::value::array({button});

        tao::json::value activity = {{"details", "Counter-Strike 1.6"}, {"assets", assets}, {"timestamps", {{"start", g_StartTime}}}};

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
                    if (!hostname.empty() && Q_UnicodeValidate(hostname.c_str()) && g_DiscordRpcServerCvar->value != 0)
                    {
                        size_t size = GeoIp_GetUtf8PrefixSize(hostname, 128);
                        activity["details"] = hostname.substr(0, size);
                    }
                    else
                    {
                        activity["details"] = Localized("#NextClient_Discord_OnServer", "On a server");
                    }

                    activity["state"] = Localized("#NextClient_Discord_Multiplayer", "Multiplayer");

                    const netadr_t& remote = cls->netchan.remote_address;
                    bool can_join = g_DiscordRpcServerCvar->value != 0 && g_DiscordRpcJoinCvar->value != 0 && remote.GetType() == NA_IP &&
                                    !remote.IsReservedAdr();

                    if (can_join)
                    {
                        std::string address = remote.ToString();
                        activity["party"]["id"] = "party-" + address;
                        activity["secrets"] = {{"join", address}};
                    }
                    else
                    {
                        activity["buttons"] = buttons;
                    }
                }

                activity["party"]["size"] = tao::json::value::array({CountPlayers(), cl->maxclients});
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
} // namespace

void DiscordPresence_Init()
{
    DiscordUrlScheme_Register(kDiscordAppId);

    g_StartTime = static_cast<int64_t>(time(nullptr));
    g_DiscordRpcCvar = gEngfuncs.pfnRegisterVariable(cvars::kDiscordRpc.name, cvars::kDiscordRpc.value, FCVAR_ARCHIVE);
    g_DiscordRpcServerCvar = gEngfuncs.pfnRegisterVariable(cvars::kDiscordRpcServer.name, cvars::kDiscordRpcServer.value, FCVAR_ARCHIVE);
    g_DiscordRpcJoinCvar = gEngfuncs.pfnRegisterVariable(cvars::kDiscordRpcJoin.name, cvars::kDiscordRpcJoin.value, FCVAR_ARCHIVE);
}

void DiscordPresence_Shutdown()
{
    g_DiscordWorker.Stop();
    DiscordHostname_Shutdown();
}

void DiscordPresence_Frame()
{
    if (g_DiscordRpcCvar->value == 0)
    {
        g_DiscordWorker.Stop();
        g_LastActivity.clear();
        DiscordHostname_Shutdown();
        return;
    }

    g_DiscordWorker.Start();

    for (const DiscordEvent& event : g_DiscordWorker.TakeEvents())
    {
        if (event.type == DiscordEvent::Type::Join)
        {
            // The session only lets through a.b.c.d:port, so nothing else can get into the command
            char command[64];
            snprintf(command, sizeof(command), "connect %s\n", event.text.c_str());
            gEngfuncs.pfnClientCmd(command);
        }

        Con_Printf("Discord: %s%s\n", event.type == DiscordEvent::Type::Join ? "join " : "", event.text.c_str());
    }

    if (*realtime < g_NextUpdateTime)
    {
        return;
    }

    g_NextUpdateTime = *realtime + 1.0;

    DiscordHostname_Update();

    std::string activity = tao::json::to_string(BuildActivity());
    if (activity != g_LastActivity)
    {
        g_LastActivity = activity;
        g_DiscordWorker.SetActivity(std::move(activity), tao::json::to_string(BuildFallbackActivity()));
    }
}
