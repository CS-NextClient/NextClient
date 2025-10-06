#include "MatchmakingService.h"

#include <optick.h>
#include <ranges>
#include <utility>

#include <ppl.h>
#include <concurrent_queue.h>
#include <strtools.h>
#include <taskcoro/TaskCoro.h>

#include "master/MasterClientFactory.h"
#include "master/MasterClientFactoryInterface.h"
#include "master/MasterClientInterface.h"

using namespace service::matchmaking;
using namespace concurrencpp;
using namespace taskcoro;

MatchmakingService::MatchmakingService(std::shared_ptr<MultiSourceQuery> source_query) :
    source_query_(std::move(source_query))
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
            if (internet_ms_client_ == nullptr)
            {
                internet_ms_client_ = internet_ms_factory_->CreateClient();
            }

            if (internet_ms_cache_client_ == nullptr)
            {
                internet_ms_cache_client_ = internet_ms_factory_->CreateCacheClient();
            }

            force_use_cache = &internet_ms_force_use_cache_;
            break;
    }

    RequestServerListResult result = co_await RequestServerListWithCacheRespect(
        internet_ms_client_,
        internet_ms_cache_client_,
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
    auto caller_ctx = SynchronizationContext::Current();
    return TaskCoro::RunInThreadPool(&MatchmakingService::RefreshServerListThreaded, this,
        gameservers,
        std::move(server_answered_callback),
        std::move(cancellation_token),
        caller_ctx);
}

result<gameserveritem_t> MatchmakingService::RefreshServer(uint32_t ip, uint16_t port)
{
    SQResponseInfo<SQ_INFO> server_info = co_await source_query_->GetInfoAsync(netadr_t(ip, port));
    co_return ConvertToGameServerItem(server_info);
}

result<MatchmakingService::RequestServerListResult> MatchmakingService::RequestServerListWithCacheRespect(
    std::shared_ptr<MasterClientInterface> ms_client,
    std::shared_ptr<MasterClientCacheInterface> ms_cache,
    bool force_use_cache,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token)
{
    std::vector<ServerInfo> servers;

    if (!force_use_cache)
    {
        servers = co_await RequestServerList(
            ms_client,
            [server_answered_callback](const ServerInfo& server_info) { server_answered_callback(server_info); },
            cancellation_token);

        if (IsServerListForcedToBeEmpty(servers))
        {
            co_return RequestServerListResult { servers, false };
        }
    }

    if (servers.empty())
    {
        servers = co_await RequestServerList(
            ms_cache,
            [server_answered_callback](const ServerInfo& server_info) { server_answered_callback(server_info); },
            cancellation_token);

        co_return RequestServerListResult { servers, true };
    }

    // TODO maybe save the cache on cancel, too?
    auto addresses = servers
        | std::views::transform(
            [](const ServerInfo& s) { return netadr_t(s.gameserver.m_NetAdr.GetIP(), s.gameserver.m_NetAdr.GetConnectionPort()); })
        | std::ranges::to<std::vector>();

    ms_cache->Save(addresses);

    co_return RequestServerListResult { servers, false };
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::RequestServerListThreaded(
    std::shared_ptr<MasterClientInterface> ms_client,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token,
    std::shared_ptr<SynchronizationContext> caller_ctx
)
{
    std::vector<ServerInfo> servers;

    std::shared_ptr<concurrency::concurrent_queue<netadr_s>> addresses_to_process = std::make_shared<concurrency::concurrent_queue<netadr_s>>();
    std::vector<SQInfoTask> server_info_tasks{};
    size_t server_index = 0;

    result<std::vector<netadr_t>> addresses_task =
        ms_client->GetServerAddressesAsync([addresses_to_process](const netadr_t& server_address)
        {
            addresses_to_process->push(server_address);
        }, cancellation_token);

    co_await source_query_->SwitchToNewSocket();

    while (addresses_task.status() == result_status::idle || !server_info_tasks.empty() || !addresses_to_process->empty())
    {
        cancellation_token->ThrowIfCancelled();

        netadr_t server_address{};
        while (server_info_tasks.size() < kMaxSimultaneousSQRequests && addresses_to_process->try_pop(server_address))
        {
            result<SQResponseInfo<SQ_INFO>> sq_task = source_query_->GetInfoAsync(server_address);
            server_info_tasks.emplace_back(server_index++, std::move(sq_task));
        }

        if (!server_info_tasks.empty())
        {
            std::vector<ServerInfo> answered_servers = co_await WaitAnySQTaskAndProcess(server_info_tasks, cancellation_token);
            servers.append_range(answered_servers);

            caller_ctx->Run([answered_servers, server_answered_callback, cancellation_token]
            {
                cancellation_token->ThrowIfCancelled();

                for (const ServerInfo& server : answered_servers)
                {
                    server_answered_callback(server);
                }
            });
        }

        co_await TaskCoro::Yield_();
    }

    co_return servers;
}

result<void> MatchmakingService::RefreshServerListThreaded(
    std::vector<gameserveritem_t> gameservers,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token,
    std::shared_ptr<SynchronizationContext> caller_ctx
)
{
    std::vector<SQInfoTask> server_info_tasks{};
    size_t current_server_index = 0;
    size_t answered_servers_count = 0;

    bool is_broadcast = !gameservers.empty() && gameservers[0].m_NetAdr.GetIP() == INADDR_BROADCAST;
    co_await source_query_->SwitchToNewSocket(is_broadcast);

    while (answered_servers_count < gameservers.size())
    {
        cancellation_token->ThrowIfCancelled();

        while (server_info_tasks.size() < kMaxSimultaneousSQRequests && current_server_index < gameservers.size())
        {
            const servernetadr_t& server_net_adr = gameservers[current_server_index].m_NetAdr;
            netadr_t server_address(server_net_adr.GetIP(), server_net_adr.GetQueryPort());

            if (server_address.GetIPHostByteOrder() == 0 && server_address.GetPortHostByteOrder() == 0)
            {
                current_server_index++;
                answered_servers_count++;
                continue;
            }

            result<SQResponseInfo<SQ_INFO>> sq_task = source_query_->GetInfoAsync(server_address);
            server_info_tasks.emplace_back(current_server_index++, std::move(sq_task));
        }

        if (!server_info_tasks.empty())
        {
            std::vector<ServerInfo> answered_servers = co_await WaitAnySQTaskAndProcess(server_info_tasks, cancellation_token);
            answered_servers_count += answered_servers.size();

            caller_ctx->Run([answered_servers, server_answered_callback, cancellation_token]
            {
                cancellation_token->ThrowIfCancelled();

                for (const ServerInfo& server : answered_servers)
                {
                    server_answered_callback(server);
                }
            });
        }

        co_await TaskCoro::Yield_();
    }
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::RequestServerList(
    std::shared_ptr<MasterClientInterface> ms_client,
    std::function<void(const ServerInfo&)> server_answered_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    auto caller_ctx = SynchronizationContext::Current();
    return TaskCoro::RunInThreadPool(&MatchmakingService::RequestServerListThreaded, this,
        ms_client,
        server_answered_callback,
        cancellation_token,
        caller_ctx);
}

result<std::vector<MatchmakingService::ServerInfo>> MatchmakingService::WaitAnySQTaskAndProcess(
    std::vector<SQInfoTask>& active_tasks,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    std::vector<ServerInfo> servers;

    if (active_tasks.empty())
    {
        co_return servers;
    }

    auto tasks = active_tasks | std::views::transform([](const SQInfoTask& req) { return req.sq_task; });
    co_await TaskCoro::WhenAny(tasks, true, cancellation_token);

    for (auto it = active_tasks.begin(); it != active_tasks.end(); )
    {
        if (it->sq_task.status() != result_status::idle)
        {
            servers.emplace_back(it->server_index, ConvertToGameServerItem(it->sq_task.get()));
            it = active_tasks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    co_return servers;
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
