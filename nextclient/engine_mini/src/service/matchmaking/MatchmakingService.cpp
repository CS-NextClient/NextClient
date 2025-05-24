#include "MatchmakingService.h"

#include <optick.h>
#include <ranges>
#include <queue>
#include <utility>

#include <strtools.h>
#include <nitro_utils/net_utils.h>
#include <taskcoro/TaskCoro.h>

#include "master/MasterClientFactory.h"
#include "master/MasterClientFactoryInterface.h"
#include "master/MasterClientInterface.h"

using namespace service::matchmaking;
using namespace concurrencpp;
using namespace taskcoro;

MatchmakingService::MatchmakingService(std::shared_ptr<MultiSourceQuery> source_query) :
    source_query_(source_query)
{
    internet_ms_factory_ = std::make_shared<MasterClientFactory>();
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::RequestServerList(
    ServerListSource server_list_source,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    std::shared_ptr<MasterClientFactoryInterface> factory;
    bool* force_use_cache;

    switch (server_list_source)
    {
        default:
        case ServerListSource::Internet:
            factory = internet_ms_factory_;
            force_use_cache = &internet_ms_force_use_cache_;
            break;
    }

    RequestServerListResult result = co_await RequestServerListWithCacheRespect(
        factory,
        *force_use_cache,
        server_answered_callback,
        cancellation_token);

    if (result.from_cache && !result.server_list.empty())
    {
        *force_use_cache = true;
    }

    co_return result.server_list;
}

result<void> MatchmakingService::RefreshServerList(
    const std::vector<gameserveritem_t>& gameservers,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    auto active_sq_tasks = std::make_shared<std::vector<SQInfoTask>>();

    int current_server_index = 0;
    int answered_servers = 0;

    bool is_broadcast = !gameservers.empty() && gameservers[0].m_NetAdr.GetIP() == INADDR_BROADCAST;

    co_await source_query_->SwitchToNewSocket(is_broadcast);

    while (answered_servers < gameservers.size())
    {
        cancellation_token->ThrowIfCancelled();

        while (active_sq_tasks->size() < kMaxSimultaneousSQRequests && current_server_index < gameservers.size())
        {
            const servernetadr_t& servernetadr = gameservers[current_server_index].m_NetAdr;
            netadr_t netadr(servernetadr.GetIP(), servernetadr.GetQueryPort());

            result<SQResponseInfo<SQ_INFO>> sq_task = source_query_->GetInfoAsync(netadr);

            active_sq_tasks->emplace_back(current_server_index++, std::move(sq_task));
        }

        if (!active_sq_tasks->empty())
        {
            ServerInfo server_info = co_await WaitAndProcessAnySQTask(active_sq_tasks, cancellation_token);
            answered_servers++;

            server_answered_callback(server_info);
        }

        co_await TaskCoro::Yield_();
    }
    co_return;
}

result<gameserveritem_t> MatchmakingService::RefreshServer(uint32_t ip, uint16_t port)
{
    SQResponseInfo<SQ_INFO> server_info = co_await source_query_->GetInfoAsync(netadr_t(ip, port));
    co_return ConvertToGameServerItem(server_info);
}

result<MatchmakingService::RequestServerListResult> MatchmakingService::RequestServerListWithCacheRespect(
    std::shared_ptr<MasterClientFactoryInterface> factory,
    bool force_use_cache,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token)
{
    std::shared_ptr<MasterClientInterface> ms_client = factory->CreateClient();

    std::vector<ServerInfo> servers;

    if (!force_use_cache)
    {
        servers = co_await RequestServerList(
            ms_client,
            [this, server_answered_callback](const ServerInfo& server_info) { server_answered_callback(server_info); },
            cancellation_token);

        if (IsServerListForcedToBeEmpty(servers))
        {
            co_return RequestServerListResult { servers, false };
        }
    }

    if (servers.empty())
    {
        std::shared_ptr<MasterClientInterface> ms_cache_client = factory->CreateCacheClient();

        servers = co_await RequestServerList(
            ms_cache_client,
            [this, server_answered_callback](const ServerInfo& server_info) { server_answered_callback(server_info); },
            cancellation_token);

        co_return RequestServerListResult { servers, true };
    }

    // TODO maybe save the cache on cancel, too?
    auto addresses = servers
        | std::views::transform(
            [](const ServerInfo& s) { return netadr_t(s.gameserver.m_NetAdr.GetIP(), s.gameserver.m_NetAdr.GetConnectionPort()); })
        | std::ranges::to<std::vector>();

    factory->CreateCacheClient()->Save(addresses);

    co_return RequestServerListResult { servers, false };
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::RequestServerList(
    std::shared_ptr<MasterClientInterface> ms_client,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    std::vector<ServerInfo> servers;

    auto addresses = std::make_shared<std::queue<netadr_t>>();
    auto active_sq_tasks = std::make_shared<std::vector<SQInfoTask>>();
    int address_index = 0;

    auto addresses_task = ms_client->GetServerAddressesAsync([this, addresses](const netadr_t& server_address)
    {
        addresses->emplace(server_address);
    }, cancellation_token);

    co_await source_query_->SwitchToNewSocket();

    while (addresses_task.status() == result_status::idle || !active_sq_tasks->empty() || !addresses->empty())
    {
        cancellation_token->ThrowIfCancelled();

        while (active_sq_tasks->size() < kMaxSimultaneousSQRequests && !addresses->empty())
        {
            netadr_t server_address = addresses->front();
            addresses->pop();

            result<SQResponseInfo<SQ_INFO>> sq_task = source_query_->GetInfoAsync(server_address);

            active_sq_tasks->emplace_back(address_index++, std::move(sq_task));
        }

        if (!active_sq_tasks->empty())
        {
            ServerInfo server_info = co_await WaitAndProcessAnySQTask(active_sq_tasks, cancellation_token);
            servers.emplace_back(std::move(server_info));

            server_answered_callback(servers.back());
        }

        co_await TaskCoro::Yield_();
    }

    co_return servers;
}

result<MatchmakingService::ServerInfo> MatchmakingService::WaitAndProcessAnySQTask(
    std::shared_ptr<std::vector<SQInfoTask>> active_tasks,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    if (active_tasks->empty())
    {
        co_return ServerInfo{};
    }

    auto tasks = *active_tasks | std::views::transform([](const SQInfoTask& req) { return req.sq_task; });
    int ready_index = co_await TaskCoro::WhenAny(tasks, cancellation_token);

    SQInfoTask active_task = active_tasks->at(ready_index);

    active_tasks->erase(active_tasks->begin() + ready_index);

    co_return ServerInfo{ active_task.server_index, ConvertToGameServerItem(active_task.sq_task.get()) };
}

bool MatchmakingService::IsServerListForcedToBeEmpty(const std::vector<ServerInfo>& servers)
{
    OPTICK_EVENT()

    if (servers.size() != 1)
    {
        return false;
    }

    const servernetadr_t& servernetadr = servers[0].gameserver.m_NetAdr;

    return servernetadr.GetIP() == 0 && servernetadr.GetConnectionPort() == 0;
}

gameserveritem_t MatchmakingService::ConvertToGameServerItem(const SQResponseInfo<SQ_INFO>& sq_info)
{
    OPTICK_EVENT()

    const netadr_t& address = sq_info.address;

    gameserveritem_t server;
    server.m_NetAdr.Init(address.GetIPHostByteOrder(), address.GetPortHostByteOrder(), address.GetPortHostByteOrder());

    if (sq_info.error_code == SQErrorCode::Ok)
    {
        const SQ_INFO& info = sq_info.value;

        server.SetName(info.hostname.c_str());
        server.m_bPassword = info.password;
        server.m_bSecure = info.secure;
        server.m_nBotPlayers = info.num_of_bots;
        server.m_nMaxPlayers = info.max_players;
        server.m_nPlayers = std::max(0, (int)info.num_players - (int)info.num_of_bots);
        server.m_nPing = (int)sq_info.ping_ms;
        server.m_bHadSuccessfulResponse = true;
        server.m_bDoNotRefresh = false;
        server.m_nAppID = info.app_id;

        V_strcpy_safe(server.m_szGameDir, info.game_directory.c_str());
        V_strcpy_safe(server.m_szMap, info.map.c_str());
        V_strcpy_safe(server.m_szGameDescription, info.game_description.c_str());
    }
    else
    {
        server.m_bHadSuccessfulResponse = false;
    }

    return server;
}
