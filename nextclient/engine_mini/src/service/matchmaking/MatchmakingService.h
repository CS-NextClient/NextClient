#pragma once
#include <concurrencpp/results/result.h>
#include <taskcoro/CancellationToken.h>
#include <steam/steam_api.h>

#include "RequestData.h"
#include "master/MasterClientFactoryInterface.h"
#include "sourcequery/MultiSourceQuery.h"

namespace service::matchmaking
{
    class MatchmakingService
    {
    public:
        struct ServerInfo
        {
            int server_index{};
            gameserveritem_t gameserver{};
        };

    private:
        struct SQInfoTask
        {
            int server_index{};
            concurrencpp::shared_result<SQResponseInfo<SQ_INFO>> sq_task{};
        };

    private:
        const int kMaxSimultaneousSQRequests = 15;

        std::shared_ptr<MasterClientFactoryInterface> internet_ms_factory_{};
        MultiSourceQuery source_query_;

    public:
        explicit MatchmakingService(int32_t server_query_timeout_ms, uint8_t server_query_retries_count);

        concurrencpp::result<std::vector<ServerInfo>> RequestInternetServerList(
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        concurrencpp::result<void> RefreshServerList(
            const std::vector<ServerInfo>& server_list,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        concurrencpp::result<gameserveritem_t> RefreshServer(uint32_t ip, uint16_t port);

    private:
        concurrencpp::result<std::vector<ServerInfo>> RequestInternetServerListInternal(
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        concurrencpp::result<std::vector<ServerInfo>> GetServersFromMaster(
            std::shared_ptr<MasterClientInterface> ms_client,
            std::function<void(const ServerInfo&)> server_answered_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        static concurrencpp::result<ServerInfo> WaitAndProcessAnySQTask(
            std::shared_ptr<std::vector<SQInfoTask>> active_tasks,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        );

        static bool IsServerListForcedToBeEmpty(const std::vector<ServerInfo>& servers);
        static gameserveritem_t ConvertToGameServerItem(const SQResponseInfo<SQ_INFO>& sq_info);
    };
}
