#pragma once
#include <memory>
#include <optional>

#include <concurrencpp/results/result.h>
#include <taskcoro/CancellationToken.h>
#include <steam/steam_api.h>

#include "RequestData.h"
#include "master/MasterClientFactoryInterface.h"
#include "service/matchmaking/master/MasterServerEntry.h"
#include "sourcequery/MultiSourceQuery.h"

namespace service::matchmaking
{
    class MatchmakingService : public std::enable_shared_from_this<MatchmakingService>
    {
    public:
        struct ServerInfo
        {
            int server_index{};
            gameserveritem_t gameserver{};

            // nullopt for a server of a refresh, which does not ask the master server
            std::optional<MasterDetails> master_details{};
        };

        enum class ServerListSource
        {
            Internet
        };
    private:
        struct SQInfoTask
        {
            int server_index{};
            concurrencpp::shared_result<SQResponseInfo<SQ_INFO>> sq_task{};
            std::optional<MasterDetails> master_details{};
        };

        struct RequestServerListResult
        {
            std::vector<ServerInfo> server_list{};
            bool from_cache{};
        };

    private:
        const int kMaxSimultaneousSQRequests = 15;

        std::shared_ptr<MultiSourceQuery> source_query_{};
        std::shared_ptr<MasterClientFactoryInterface> internet_ms_factory_{};
        std::shared_ptr<MasterClientInterface> internet_ms_client_{};
        std::shared_ptr<MasterClientCacheInterface> internet_ms_cache_client_{};
        bool internet_ms_force_use_cache_{};

        explicit MatchmakingService(std::shared_ptr<MultiSourceQuery> source_query);

    public:
        // master_list_callback runs on the caller's context with each list the request reads, the master server's and,
        // on a fallback, the cache's, as soon as that list is complete
        concurrencpp::result<std::vector<ServerInfo>> RequestServerList(
            ServerListSource server_list_source,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::function<void(const std::vector<MasterServerEntry>&)> master_list_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        concurrencpp::result<void> RefreshServerList(
            const std::vector<gameserveritem_t>& gameservers,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        concurrencpp::result<gameserveritem_t> RefreshServer(uint32_t ip, uint16_t port);

    private:
        concurrencpp::result<RequestServerListResult> RequestServerListWithCacheRespect(
            std::shared_ptr<MasterClientInterface> ms_client,
            std::shared_ptr<MasterClientCacheInterface> ms_cache,
            bool force_use_cache,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::function<void(const std::vector<MasterServerEntry>&)> master_list_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        concurrencpp::result<std::vector<ServerInfo>> RequestServerListThreaded(
            std::shared_ptr<MasterClientInterface> ms_client,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::function<void(const std::vector<MasterServerEntry>&)> master_list_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token,
            std::shared_ptr<taskcoro::SynchronizationContext> caller_ctx
        );

        concurrencpp::result<void> RefreshServerListThreaded(
            std::vector<gameserveritem_t> gameservers,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token,
            std::shared_ptr<taskcoro::SynchronizationContext> caller_ctx
        );

        concurrencpp::result<std::vector<ServerInfo>> RequestServerList(
            std::shared_ptr<MasterClientInterface> ms_client,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::function<void(const std::vector<MasterServerEntry>&)> master_list_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        static concurrencpp::result<std::vector<ServerInfo>> WaitAnySQTaskAndProcess(
            std::vector<SQInfoTask>& active_tasks,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        static bool IsServerListForcedToBeEmpty(const std::vector<ServerInfo>& servers);
        static gameserveritem_t ConvertToGameServerItem(const SQResponseInfo<SQ_INFO>& sq_info);

    public:
        static std::shared_ptr<MatchmakingService> Create(std::shared_ptr<MultiSourceQuery> source_query);
    };
}
