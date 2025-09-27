#include "MatchmakingSteamComp.h"

#include <optick.h>
#include <strtools.h>

using namespace service::matchmaking;
using namespace concurrencpp;
using namespace taskcoro;

MatchmakingSteamComp::MatchmakingSteamComp()
{
    source_query_ = std::make_shared<MultiSourceQuery>(750, 3);
    matchmaking_service_ = std::make_shared<MatchmakingService>(source_query_);
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
    return &request_data.servers[server_id];
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
        co_await RefreshServerList(request_id, request_data.servers, request_data.response_callback, ct);
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

    TaskCoro::RunInMainThread([this](HServerListRequest request_id, int server_id, std::shared_ptr<CancellationToken> ct) -> result<void>
    {
        ct->ThrowIfCancelled();

        auto& request_data = std::get<ServerListRequestData>(server_requests_[request_id]);
        servernetadr_t net_addr = request_data.servers[server_id].m_NetAdr;

        gameserveritem_t gameserver = co_await matchmaking_service_->RefreshServer(net_addr.GetIP(), net_addr.GetQueryPort());
        ct->ThrowIfCancelled();

        request_data = std::get<ServerListRequestData>(server_requests_[request_id]);

        if (gameserver.m_bHadSuccessfulResponse)
        {
            gameserver.m_ulTimeLastPlayed = request_data.servers[server_id].m_ulTimeLastPlayed;
            request_data.servers[server_id] = gameserver;

            OPTICK_EVENT("MatchmakingSteamComp::RefreshServer - response_callback->ServerResponded")
            request_data.response_callback->ServerResponded(request_id, server_id);
        }
        else
        {
            OPTICK_EVENT("MatchmakingSteamComp::RefreshServer - response_callback->ServerFailedToRespond")
            request_data.response_callback->ServerFailedToRespond(request_id, server_id);
        }
    }, request_id, server_id, request_data.cancellation_token);
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
    EMatchMakingServerResponse response_code = eServerResponded;

    co_await matchmaking_service_->RequestServerList(
        server_list_source,
        [this, request_id, response_callback] (const MatchmakingService::ServerInfo& server_info)
        {
            ServerAnsweredHandler(request_id, response_callback, server_info);
        }, ct);

    OPTICK_EVENT("MatchmakingSteamComp::RequestServerList - response_callback->RefreshComplete")
    response_callback->RefreshComplete(request_id, response_code);
}

result<void> MatchmakingSteamComp::RefreshServerList(
    HServerListRequest request_id,
    const std::vector<gameserveritem_t>& gameservers,
    ISteamMatchmakingServerListResponse* response_callback,
    std::shared_ptr<CancellationToken> ct
)
{
    EMatchMakingServerResponse response_code = eServerResponded;

    co_await matchmaking_service_->RefreshServerList(
        gameservers,
        [this, request_id, response_callback] (const MatchmakingService::ServerInfo& server_info)
        {
            ServerAnsweredHandler(request_id, response_callback, server_info);
        }, ct);

    OPTICK_EVENT("MatchmakingSteamComp::RefreshServerList - response_callback->RefreshComplete")
    response_callback->RefreshComplete(request_id, response_code);
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
                InitEmptyGameServerItem(request_data.servers[i], 0, 0);
            }
        }

        request_data.servers[server_info.server_index] = server_info.gameserver;
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
