#include "MatchmakingSteamComp.h"

#include <optick.h>
#include <strtools.h>

using namespace service::matchmaking;
using namespace concurrencpp;
using namespace taskcoro;

MatchmakingSteamComp::MatchmakingSteamComp() :
    matchmaking_service_(750, 3)
{

}

HServerListRequest MatchmakingSteamComp::RequestInternetServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* response_callback
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto servers_request_data = ServerListRequestData(request_id, response_callback);
    auto ct = servers_request_data.cancellation_token;

    server_requests_.emplace(request_id, std::move(servers_request_data));

    TaskCoro::RunInMainThread<void>(
        [this, request_id_param = request_id, response_callback_param = response_callback, ct_param = ct]
        () -> result<void>
        {
            auto response_callback = response_callback_param;
            auto request_id = request_id_param;
            auto ct = ct_param;

            EMatchMakingServerResponse response_code = eServerResponded;

            try
            {
                co_await matchmaking_service_.RequestInternetServerList(
                    [this, request_id, response_callback]
                    (const MatchmakingService::ServerInfo& server_info)
                    {
                        OPTICK_EVENT("MatchmakingSteamComp::RequestInternetServerList - server callback")
                        auto& request = server_requests_[request_id];

                        if (std::holds_alternative<ServerListRequestData>(request))
                        {
                            ServerListRequestData& request_data = std::get<ServerListRequestData>(request);

                            if (server_info.server_index >= request_data.servers.size())
                            {
                                request_data.servers.resize(server_info.server_index + 1);

                                for (gameserveritem_t& server : request_data.servers)
                                {
                                    InitEmptyGameServerItem(server);
                                }
                            }

                            request_data.servers[server_info.server_index] = server_info.gameserver;
                        }

                        if (server_info.gameserver.m_bHadSuccessfulResponse)
                        {
                            OPTICK_EVENT("MatchmakingSteamComp::RequestInternetServerList - server callback - ServerResponded")
                            response_callback->ServerResponded(request_id, server_info.server_index);
                        }
                        else
                        {
                            OPTICK_EVENT("MatchmakingSteamComp::RequestInternetServerList - server callback - ServerFailedToRespond")
                            response_callback->ServerFailedToRespond(request_id, server_info.server_index);
                        }
                    }, ct);
            }
            catch (OperationCanceledException&)
            { }

            OPTICK_EVENT("MatchmakingSteamComp::RequestInternetServerList, RefreshComplete callback")
            response_callback->RefreshComplete(request_id, response_code);
        });

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestLANServerList(AppId_t iApp, ISteamMatchmakingServerListResponse* pRequestServersResponse)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(pRequestServersResponse, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestLANServerList(iApp, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, pRequestServersResponse, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestFriendsServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* pRequestServersResponse
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(pRequestServersResponse, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestFriendsServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, pRequestServersResponse, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestFavoritesServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* pRequestServersResponse
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(pRequestServersResponse, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestFavoritesServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, pRequestServersResponse, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestHistoryServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters,
    ISteamMatchmakingServerListResponse* pRequestServersResponse
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(pRequestServersResponse, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestHistoryServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, pRequestServersResponse, steam_request_id, steam_response_proxy));

    return request_id;
}

HServerListRequest MatchmakingSteamComp::RequestSpectatorServerList(
    AppId_t iApp,
    MatchMakingKeyValuePair_t** ppchFilters,
    uint32 nFilters,
    ISteamMatchmakingServerListResponse* pRequestServersResponse
)
{
    auto request_id = (HServerListRequest)++server_list_request_counter_;

    auto steam_response_proxy = new SteamMatchmakingServerListResponseProxy(pRequestServersResponse, request_id);
    auto steam_request_id = SteamMatchmakingServers()->RequestSpectatorServerList(iApp, ppchFilters, nFilters, steam_response_proxy);

    server_requests_.emplace(request_id, SteamServersListRequestData(request_id, pRequestServersResponse, steam_request_id, steam_response_proxy));

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

gameserveritem_t* MatchmakingSteamComp::GetServerDetails(HServerListRequest request_id, int iServer)
{
    if (!server_requests_.contains(request_id))
    {
        return nullptr;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->GetServerDetails(request_data.steam_request_id, iServer);
    }

    auto& request_data = std::get<ServerListRequestData>(request);
    return &request_data.servers[iServer];
}

void MatchmakingSteamComp::CancelQuery(HServerListRequest request_id)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        return SteamMatchmakingServers()->CancelQuery(request_data.steam_request_id);
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

    TaskCoro::RunInMainThread<void>(
        [this, request_id_param = request_id, response_callback_param = request_data.response_callback, ct_param = request_data.cancellation_token]
        () -> result<void>
        {
            auto response_callback = response_callback_param;
            auto request_id = request_id_param;
            auto ct = ct_param;

            EMatchMakingServerResponse response_code = eServerResponded;

            try
            {
                co_await matchmaking_service_.RequestInternetServerList(
                    [this, request_id, response_callback]
                    (const MatchmakingService::ServerInfo& server_info)
                    {
                        OPTICK_EVENT("MatchmakingSteamComp::RefreshQuery - server callback")
                        auto& request = server_requests_[request_id];

                        if (std::holds_alternative<ServerListRequestData>(request))
                        {
                            auto& request_data = std::get<ServerListRequestData>(request);
                            request_data.servers[server_info.server_index] = server_info.gameserver;
                        }

                        if (server_info.gameserver.m_bHadSuccessfulResponse)
                        {
                            OPTICK_EVENT("MatchmakingSteamComp::RefreshQuery - server callback - ServerResponded")
                            response_callback->ServerResponded(request_id, server_info.server_index);
                        }
                        else
                        {
                            OPTICK_EVENT("MatchmakingSteamComp::ServerFailedToRespond - server callback - ServerFailedToRespond")
                            response_callback->ServerFailedToRespond(request_id, server_info.server_index);
                        }
                    }, ct);
            }
            catch (OperationCanceledException&)
            { }

            OPTICK_EVENT("MatchmakingSteamComp::ServerFailedToRespond - server callback - RefreshComplete")
            response_callback->RefreshComplete(request_id, response_code);
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

void MatchmakingSteamComp::RefreshServer(HServerListRequest request_id, int iServer)
{
    if (!server_requests_.contains(request_id))
    {
        return;
    }

    auto& request = server_requests_[request_id];

    if (std::holds_alternative<SteamServersListRequestData>(request))
    {
        auto& request_data = std::get<SteamServersListRequestData>(request);
        SteamMatchmakingServers()->RefreshServer(request_data.steam_request_id, iServer);
        return;
    }

    TaskCoro::RunInMainThread<void>([this, request_id, iServer]() -> result<void>
    {
        int server_index = iServer;

        auto& request_data = std::get<ServerListRequestData>(server_requests_[request_id]);
        auto net_addr = request_data.servers[server_index].m_NetAdr;

        auto gameserver = co_await matchmaking_service_.RefreshServer(net_addr.GetIP(), net_addr.GetQueryPort());

        request_data.servers[server_index] = gameserver;
    });
}

HServerQuery MatchmakingSteamComp::PingServer(uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse* pRequestServersResponse)
{
    return SteamMatchmakingServers()->PingServer(unIP, usPort, pRequestServersResponse);
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

void MatchmakingSteamComp::InitEmptyGameServerItem(gameserveritem_t& gameserveritem)
{
    OPTICK_EVENT()

    if (app_id_ == 0)
    {
        app_id_ = SteamUtils()->GetAppID();
    }

    gameserveritem.m_NetAdr.Init(0, 0, 0);
    gameserveritem.m_nAppID = app_id_;
    V_strcpy_safe(gameserveritem.m_szGameDir, "cstrike");
    V_strcpy_safe(gameserveritem.m_szMap, "-");
    V_strcpy_safe(gameserveritem.m_szGameDescription, "-");
}
