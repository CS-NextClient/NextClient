#pragma once
#include <utility>
#include <vector>
#include <unordered_map>
#include <future>

#include <taskcoro/TaskCoro.h>
#include <data_types/ByteBuffer.h>
#include <netadr.h>
#include <taskcoro/impl/SynchronizationContextManual.h>

#include "SourceQueryInterface.h"
#include "SQResponseInfo.h"
#include "source_query_types.h"
#include "netadr_hasher.h"

namespace service::matchmaking
{
    class MatchmakingService;
}

class MultiSourceQuery
{
    struct RecvData;
    struct SourceQueryData;
    struct SocketData;

    const int kMaxBufferHoldsTimeMs = 5000;
    const int kSocketLifeTimeMs = 5000;

    std::shared_ptr<taskcoro::SynchronizationContextManual> manual_sync_ctx_;
    std::shared_ptr<taskcoro::SynchronizationContext> sync_ctx_;
    std::shared_ptr<taskcoro::CancellationToken> cancellation_token_;
    std::atomic_bool main_loop_done_{};

    std::vector<SocketData> sockets_{};
    uint32_t timeout_ms_{};
    uint8_t retries_{};

    std::unordered_map<netadr_t, std::vector<SourceQueryData>> queries_{};
    std::unordered_map<netadr_t, std::vector<RecvData>> recv_{};

public:
    explicit MultiSourceQuery(int32_t timeout, uint8_t retries);
    ~MultiSourceQuery();

    [[nodiscard]] concurrencpp::result<SQResponseInfo<SQ_INFO>> GetInfoAsync(netadr_t address);
    [[nodiscard]] concurrencpp::result<SQResponseInfo<SQ_RULES>> GetRulesAsync(netadr_t address);
    [[nodiscard]] concurrencpp::result<SQResponseInfo<SQ_PLAYERS>> GetPlayersAsync(netadr_t address);

    concurrencpp::result<void> SwitchToNewSocket(bool broadcast = false);

private:
    void MainLoop();

    void AssembleBuffer(ByteBuffer& buffer, netadr_t from_addr);
    void ProcessIncomingBuffers(std::chrono::high_resolution_clock::time_point current_time);
    void ProcessQueries(netadr_t address, ByteBuffer& buffer);
    void ProcessTimeoutQueries(std::chrono::high_resolution_clock::time_point current_time);
    void ReceiveAndAssembleBuffers();
    void CloseExpiredSockets(std::chrono::high_resolution_clock::time_point current_time);
    void CreateSocket(bool broadcast);
    void CreateSocketIfNeeded(bool broadcast);

    [[nodiscard]] static uint32_t GetElapsedMs(
        std::chrono::high_resolution_clock::time_point current,
        std::chrono::high_resolution_clock::time_point past
    )
    { return std::chrono::duration_cast<std::chrono::milliseconds>(current - past).count(); }

    struct RecvData
    {
        int32_t requestid = -1;
        ByteBuffer packets[16]{};
        uint8_t numpackets{};
        uint8_t received{};

        std::chrono::high_resolution_clock::time_point first_packet_time{};

        bool recv_done{};
        bool has_error{};

        ByteBuffer recv_buffer{};

        explicit RecvData(std::chrono::high_resolution_clock::time_point time) :
            recv_done(false),
            has_error(false),
            first_packet_time(time)
        { }

        explicit RecvData(ByteBuffer buffer, std::chrono::high_resolution_clock::time_point time) :
            recv_buffer(std::move(buffer)),
            recv_done(true),
            has_error(false),
            first_packet_time(time)
        { }
    };

    struct SourceQueryData
    {
        std::unique_ptr<SourceQueryInterface> query{};
        std::chrono::high_resolution_clock::time_point send_time{};
        uint8_t retry = 1;

        explicit SourceQueryData() = default;
        explicit SourceQueryData(std::unique_ptr<SourceQueryInterface>&& query, std::chrono::high_resolution_clock::time_point send_time) :
            query(std::move(query)),
            send_time(send_time)
        { }
    };

    struct SocketData
    {
        SOCKET socket{};
        std::chrono::high_resolution_clock::time_point close_time{};

        explicit SocketData() = default;
        explicit SocketData(SOCKET socket, std::chrono::high_resolution_clock::time_point close_time) :
            socket(socket),
            close_time(close_time)
        { }
    };
};
