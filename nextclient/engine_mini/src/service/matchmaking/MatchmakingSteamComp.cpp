#include "MatchmakingSteamComp.h"

#include "engine.h"

#include <cassert>
#include <ranges>

#include <easylogging++.h>
#include <optick.h>
#include <strtools.h>

#include "service/geoip/GeoIpLanguage.h"
#include "service/matchmaking/ServerCountry.h"

using namespace service::matchmaking;
using namespace concurrencpp;
using namespace taskcoro;

namespace
{
    constexpr const char* kGeoIpDatabaseFile = "servers/geoip_country.mmdb";
    constexpr const char* kGameModeRulesFile = "servers/game_mode_rules.json";

    // the shipped copy only: the game and download directories come first in the search order
    constexpr const char* kGameModeRulesPathId = "PLATFORM";
    constexpr unsigned int kMaxGameModeRulesFileSize = 1024 * 1024;
} // namespace

MatchmakingSteamComp::MatchmakingSteamComp()
{
    ct_ = CancellationToken::Create();

    source_query_ = std::make_shared<MultiSourceQuery>(750, 3);
    matchmaking_service_ = MatchmakingService::Create(source_query_);
}

MatchmakingSteamComp::~MatchmakingSteamComp()
{
    ct_->SetCanceled();

    for (auto& [request_id, request] : server_requests_)
    {
        MatchmakingSteamComp::CancelQuery(request_id);
    }
}

HServerListRequest MatchmakingSteamComp::RequestInternetServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto ct = CancellationToken::Create();
    auto servers_request_data = ServerListRequestData(request_id, response_callback, ct);

    server_requests_.emplace(request_id, std::move(servers_request_data));

    TaskCoro::RunInMainThread([this, request_id, response_callback, ct] () -> result<void>
    {
        ct->ThrowIfCancelled();
        co_await RequestServerList(request_id, MatchmakingService::ServerListSource::Internet, response_callback, ct);
    });

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestLANServerList(AppId_t iApp, ISteamMatchmakingServerListResponse* response_callback)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestLANServerList(iApp, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestFriendsServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestFriendsServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestFavoritesServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestFavoritesServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestHistoryServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestHistoryServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestSpectatorServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(response_callback, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestSpectatorServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, response_callback, steam_request_id, steam_response_proxy));

    return request_id;
}

void MatchmakingSteamComp::ReleaseRequest(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);

        SteamMatchmakingServers()->ReleaseRequest(request_data.steam_request_id);
        delete request_data.steam_response_callback;
    }
    else
    {
        auto& request_data = std::get<ServerListRequestData>(request);
        request_data.cancellation_token->SetCanceled();
    }

    server_requests_.erase(request_id);
}

gameserveritem_t* MatchmakingSteamComp::GetServerDetails(HServerListRequest request_id, int server_id)
{
    if (!server_requests_.contains(request_id))
    {
        return nullptr;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->GetServerDetails(request_data.steam_request_id, server_id);
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    return &request_data.servers[server_id].gameserver;
}

void MatchmakingSteamComp::CancelQuery(HServerListRequest request_id)
{
    OPTICK_EVENT();
    
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        SteamMatchmakingServers()->CancelQuery(request_data.steam_request_id);
        return;
    }

    if (!IsRefreshing(request_id))
    {
        return;
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    request_data.in_progress = false;
    request_data.cancellation_token->SetCanceled();
    request_data.response_callback->RefreshComplete(request_id, eServerResponded);
}

void MatchmakingSteamComp::RefreshQuery(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        SteamMatchmakingServers()->RefreshQuery(request_data.steam_request_id);
        return;
    }

    if (IsRefreshing(request_id))
    {
        return;
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    request_data.cancellation_token->SetCanceled();
    request_data.cancellation_token = CancellationToken::Create();
    request_data.in_progress = true;

    TaskCoro::RunInMainThread([this, request_id, ct = request_data.cancellation_token] () -> result<void>
    {
        ct->ThrowIfCancelled();

        auto& request_data = std::get<ServerListRequestData>(server_requests_[request_id]);
        std::vector<gameserveritem_t> gameservers = request_data.servers
            | std::views::transform([](const ServerListEntry& entry) { return entry.gameserver; })
            | std::ranges::to<std::vector>();

        co_await RefreshServerList(request_id, gameservers, request_data.response_callback, ct);
    });
}

bool MatchmakingSteamComp::IsRefreshing(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return false;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->IsRefreshing(request_data.steam_request_id);
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    return request_data.in_progress;
}

int MatchmakingSteamComp::GetServerCount(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return 0;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->GetServerCount(request_data.steam_request_id);
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    return request_data.servers.size();
}

void MatchmakingSteamComp::RefreshServer(HServerListRequest request_id, int server_id)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        SteamMatchmakingServers()->RefreshServer(request_data.steam_request_id, server_id);
        return;
    }

    auto& request_data = std::get<ServerListRequestData>(server_requests_[request_id]);

    TaskCoro::RunInMainThread([this, shutdown_ct = ct_, request_id, server_id, ct = request_data.cancellation_token]() -> result<void>
    {
        shutdown_ct->ThrowIfCancelled();
        ct->ThrowIfCancelled();

        servernetadr_t net_addr = std::get<ServerListRequestData>(server_requests_[request_id]).servers[server_id].gameserver.m_NetAdr;

        gameserveritem_t gameserver = co_await matchmaking_service_->RefreshServer(net_addr.GetIP(), net_addr.GetQueryPort());
        shutdown_ct->ThrowIfCancelled();
        ct->ThrowIfCancelled();

        if (!server_requests_.contains(request_id))
        {
            co_return;
        }

        auto& request_data = std::get<ServerListRequestData>(server_requests_[request_id]);

        if (gameserver.m_bHadSuccessfulResponse)
        {
            gameserver.m_ulTimeLastPlayed = request_data.servers[server_id].gameserver.m_ulTimeLastPlayed;
            request_data.servers[server_id].gameserver = gameserver;

            OPTICK_EVENT("MatchmakingSteamComp::RefreshServer - response_callback->ServerResponded")
            request_data.response_callback->ServerResponded(request_id, server_id);
        }
        else
        {
            OPTICK_EVENT("MatchmakingSteamComp::RefreshServer - response_callback->ServerFailedToRespond")
            request_data.response_callback->ServerFailedToRespond(request_id, server_id);
        }
    });
}

HServerQuery MatchmakingSteamComp::PingServer(uint32 ip, uint16 port, ISteamMatchmakingPingResponse* response_callback)
{
    return SteamMatchmakingServers()->PingServer(ip, port, response_callback);
}

HServerQuery MatchmakingSteamComp::PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse* pRequestServersResponse)
{
    return SteamMatchmakingServers()->PlayerDetails(unIP, usPort, pRequestServersResponse);
}

HServerQuery MatchmakingSteamComp::ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse* pRequestServersResponse)
{
    return SteamMatchmakingServers()->ServerRules(unIP, usPort, pRequestServersResponse);
}

void MatchmakingSteamComp::CancelServerQuery(HServerQuery hServerQuery)
{
    SteamMatchmakingServers()->CancelServerQuery(hServerQuery);
}

result<void> MatchmakingSteamComp::RequestServerList(
    HServerListRequest request_id,
    MatchmakingService::ServerListSource server_list_source,
    ISteamMatchmakingServerListResponse* response_callback,
    std::shared_ptr<CancellationToken> ct
)
{
    co_await matchmaking_service_->RequestServerList(
        server_list_source,
        [this, request_id, response_callback] (const MatchmakingService::ServerInfo& server_info)
        {
            ServerAnsweredHandler(request_id, response_callback, server_info);
        },
        [this] (const std::vector<MasterServerEntry>& entries)
        {
            master_details_by_address_.Rebuild(entries);
        }, ct);

    // server_requests_ and the response callback are main-thread confined
    assert(TaskCoro::IsMainThread());

    OPTICK_EVENT("MatchmakingSteamComp::RequestServerList - response_callback->RefreshComplete")
    response_callback->RefreshComplete(request_id, eServerResponded);
}

result<void> MatchmakingSteamComp::RefreshServerList(
    HServerListRequest request_id,
    const std::vector<gameserveritem_t>& gameservers,
    ISteamMatchmakingServerListResponse* response_callback,
    std::shared_ptr<CancellationToken> ct
)
{
    co_await matchmaking_service_->RefreshServerList(
        gameservers,
        [this, request_id, response_callback] (const MatchmakingService::ServerInfo& server_info)
        {
            ServerAnsweredHandler(request_id, response_callback, server_info);
        }, ct);

    assert(TaskCoro::IsMainThread());

    OPTICK_EVENT("MatchmakingSteamComp::RefreshServerList - response_callback->RefreshComplete")
    response_callback->RefreshComplete(request_id, eServerResponded);
}

void MatchmakingSteamComp::ServerAnsweredHandler(
    HServerListRequest request_id,
    ISteamMatchmakingServerListResponse* response_callback,
    const MatchmakingService::ServerInfo& server_info)
{
    OPTICK_EVENT("MatchmakingSteamComp::ServerAnsweredHandler")

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<ServerListRequestData>(request))
    {
        auto& request_data = std::get<ServerListRequestData>(request);

        size_t server_count = request_data.servers.size();

        if (server_info.server_index >= server_count)
        {
            request_data.servers.resize(server_info.server_index + 1);

            for (size_t i = server_count; i < request_data.servers.size(); ++i)
            {
                InitEmptyGameServerItem(request_data.servers[i].gameserver, 0, 0);
            }
        }

        ServerListEntry& entry = request_data.servers[server_info.server_index];
        entry.gameserver = server_info.gameserver;

        if (server_info.master_details.has_value())
        {
            entry.master_details = *server_info.master_details;
        }
    }

    if (server_info.gameserver.m_bHadSuccessfulResponse)
    {
        OPTICK_EVENT("MatchmakingSteamComp::ServerAnsweredHandler - ServerResponded")
        response_callback->ServerResponded(request_id, server_info.server_index);
    }
    else
    {
        OPTICK_EVENT("MatchmakingSteamComp::ServerAnsweredHandler - ServerFailedToRespond")
        response_callback->ServerFailedToRespond(request_id, server_info.server_index);
    }
}

void MatchmakingSteamComp::InitEmptyGameServerItem(gameserveritem_t& gameserver, uint32_t ip, uint16_t port)
{
    OPTICK_EVENT()

    if (app_id_ == 0)
    {
        app_id_ = SteamUtils()->GetAppID();
    }

    gameserver.m_NetAdr.Init(ip, port, port);
    gameserver.m_nAppID = app_id_;
    V_strcpy_safe(gameserver.m_szGameDir, "cstrike");
    V_strcpy_safe(gameserver.m_szMap, "-");
    V_strcpy_safe(gameserver.m_szGameDescription, "-");
}

bool MatchmakingSteamComp::GetServerDetailsNext(HServerListRequest request_id, int server_index, ServerDetailsNext* out)
{
    *out = ServerDetailsNext{};

    if (!server_requests_.contains(request_id))
    {
        return false;
    }

    auto& request = server_requests_[request_id];
    const gameserveritem_t* gameserver;
    const MasterDetails* master_details;

    if (std::holds_alternative<ServerListRequestData>(request))
    {
        const ServerListRequestData& request_data = std::get<ServerListRequestData>(request);

        if (server_index < 0 || server_index >= static_cast<int>(request_data.servers.size()))
        {
            return false;
        }

        const ServerListEntry& entry = request_data.servers[server_index];
        gameserver = &entry.gameserver;
        master_details = &entry.master_details;
    }
    else
    {
        const SteamServersListRequestData& request_data = std::get<SteamServersListRequestData>(request);
        gameserver = SteamMatchmakingServers()->GetServerDetails(request_data.steam_request_id, server_index);

        if (gameserver == nullptr)
        {
            return false;
        }

        master_details = master_details_by_address_.Find(gameserver->m_NetAdr.GetIP(), gameserver->m_NetAdr.GetConnectionPort());
    }

    FillGameMode(*gameserver, master_details != nullptr ? master_details->game_mode.c_str() : "", out);
    FillCountry(gameserver->m_NetAdr.GetIP(), master_details != nullptr ? master_details->country_code.c_str() : "", out);

    return true;
}

void MatchmakingSteamComp::FillGameMode(const gameserveritem_t& gameserver, const char* master_game_mode, ServerDetailsNext* out)
{
    if (master_game_mode[0])
    {
        V_strcpy_safe(out->game_mode, master_game_mode);
        return;
    }

    if (EnsureGameModeRulesLoaded())
    {
        V_strcpy_safe(out->game_mode, game_mode_rules_.Detect(gameserver.m_szGameDescription, gameserver.GetName(), gameserver.m_szMap));
    }
}

bool MatchmakingSteamComp::EnsureGameModeRulesLoaded()
{
    if (game_mode_rules_load_attempted_)
    {
        return game_mode_rules_.is_loaded();
    }

    game_mode_rules_load_attempted_ = true;

    FileHandle_t file = g_pFileSystem->Open(kGameModeRulesFile, "rb", kGameModeRulesPathId);

    if (!file)
    {
        LOG(WARNING) << "[MatchmakingSteamComp] Game mode rules " << kGameModeRulesFile
                     << " not found, modes the master omits stay unknown";
        return false;
    }

    unsigned int size = g_pFileSystem->Size(file);

    if (size > kMaxGameModeRulesFileSize)
    {
        g_pFileSystem->Close(file);

        LOG(ERROR) << "[MatchmakingSteamComp] Game mode rules " << kGameModeRulesFile << " are larger than " << kMaxGameModeRulesFileSize
                   << " bytes, modes the master omits stay unknown";
        return false;
    }

    std::string text(size, '\0');
    int read = g_pFileSystem->Read(text.data(), static_cast<int>(text.size()), file);
    g_pFileSystem->Close(file);

    text.resize(read > 0 ? static_cast<size_t>(read) : 0);

    if (!game_mode_rules_.Parse(text))
    {
        LOG(ERROR) << "[MatchmakingSteamComp] Game mode rules " << kGameModeRulesFile
                   << " are malformed, modes the master omits stay unknown";
        return false;
    }

    return true;
}

void MatchmakingSteamComp::FillCountry(uint32_t ip, const char* master_country_code, ServerDetailsNext* out)
{
    GeoIpCountry country;
    bool resolved = EnsureGeoIpDatabaseOpened() && geoip_.ResolveCountry(ip, geoip_language_.c_str(), country);

    if (resolved && country.name[0])
    {
        country_names_by_code_.emplace(country.code, country.name);
    }

    ServerCountry_Fill(master_country_code, resolved ? &country : nullptr, country_names_by_code_, out);
}

bool MatchmakingSteamComp::EnsureGeoIpDatabaseOpened()
{
    if (geoip_open_attempted_)
    {
        return geoip_.is_open();
    }

    geoip_open_attempted_ = true;

    char path[MAX_PATH];

    if (g_pFileSystem->GetLocalPath(kGeoIpDatabaseFile, path, sizeof(path)) == nullptr)
    {
        LOG(WARNING) << "[MatchmakingSteamComp] GeoIP database " << kGeoIpDatabaseFile << " not found, server countries stay unknown";
        return false;
    }

    if (!geoip_.Open(path))
    {
        LOG(ERROR) << "[MatchmakingSteamComp] Failed to open GeoIP database " << path;
        return false;
    }

    geoip_language_ = GeoIp_GetNamesLanguage(SteamApps()->GetCurrentGameLanguage());

    return true;
}
