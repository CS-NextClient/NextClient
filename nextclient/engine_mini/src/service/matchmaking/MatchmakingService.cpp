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

MatchmakingService::MatchmakingService(int32_t server_query_timeout_ms, uint8_t server_query_retries_count) :
    source_query_(server_query_timeout_ms, server_query_retries_count)
{
    internet_ms_factory_ = std::make_shared<MasterClientFactory>();
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::RequestInternetServerList(
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    return RequestInternetServerListInternal(std::move(server_answered_callback), std::move(cancellation_token));
}

result<void> MatchmakingService::RefreshServerList(
    const std::vector<ServerInfo>& server_list,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    auto active_sq_tasks = std::make_shared<std::vector<SQInfoTask>>();

    int current_server_index = 0;
    int answered_servers = 0;

    bool is_broadcast = !server_list.empty() && server_list[0].gameserver.m_NetAdr.GetIP() == INADDR_BROADCAST;

    co_await source_query_.SwitchToNewSocket(is_broadcast);

    while (answered_servers != server_list.size())
    {
        cancellation_token->ThrowIfCancelled();

        while (active_sq_tasks->size() < kMaxSimultaneousSQRequests)
        {
            const servernetadr_t& servernetadr = server_list[current_server_index].gameserver.m_NetAdr;
            netadr_t netadr(servernetadr.GetIP(), servernetadr.GetQueryPort());

            result<SQResponseInfo<SQ_INFO>> sq_task = source_query_.GetInfoAsync(netadr);

            active_sq_tasks->emplace_back(current_server_index++, std::move(sq_task));
        }

        if (!active_sq_tasks->empty())
        {
            ServerInfo server_info = co_await WaitAndProcessAnySQTask(active_sq_tasks, cancellation_token);
            server_answered_callback(server_info);
        }

        co_await TaskCoro::Yield_();
    }
    co_return;
}

result<gameserveritem_t> MatchmakingService::RefreshServer(uint32_t ip, uint16_t port)
{
    SQResponseInfo<SQ_INFO> server_info = co_await source_query_.GetInfoAsync(netadr_t(ip, port));
    co_return ConvertToGameServerItem(server_info);
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::RequestInternetServerListInternal(
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    std::shared_ptr<MasterClientInterface> ms_client = internet_ms_factory_->CreateClient();

    std::vector<ServerInfo> servers = co_await GetServersFromMaster(
        ms_client,
        [this, server_answered_callback](const ServerInfo& server_info) { server_answered_callback(server_info); },
        cancellation_token);

    if (IsServerListForcedToBeEmpty(servers))
    {
        co_return servers;
    }

    if (servers.empty())
    {
        std::shared_ptr<MasterClientInterface> ms_cache_client = internet_ms_factory_->CreateCacheClient();

        co_return co_await GetServersFromMaster(
            ms_cache_client,
            [this, server_answered_callback](const ServerInfo& server_info) { server_answered_callback(server_info); },
            cancellation_token);
    }

    if (!servers.empty())
    {
        // TODO maybe save the cache on cancel, too?
        auto addresses = servers
            | std::views::transform(
                [](const ServerInfo& s) { return netadr_t(s.gameserver.m_NetAdr.GetIP(), s.gameserver.m_NetAdr.GetConnectionPort()); })
            | std::ranges::to<std::vector>();

        internet_ms_factory_->CreateCacheClient()->Save(addresses);
    }

    co_return servers;
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::GetServersFromMaster(
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

    co_await source_query_.SwitchToNewSocket();

    while (addresses_task.status() == result_status::idle || !active_sq_tasks->empty() || !addresses->empty())
    {
        cancellation_token->ThrowIfCancelled();

        while (active_sq_tasks->size() < kMaxSimultaneousSQRequests && !addresses->empty())
        {
            netadr_t server_address = addresses->front();
            addresses->pop();

            result<SQResponseInfo<SQ_INFO>> sq_task = source_query_.GetInfoAsync(server_address);

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
