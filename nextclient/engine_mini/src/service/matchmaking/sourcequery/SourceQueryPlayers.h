#pragma once
#include <taskcoro/TaskCoro.h>
#include <netadr.h>

#include "SourceQueryInterface.h"
#include "SQResponseInfo.h"
#include "source_query_types.h"

class SourceQueryPlayers : public SourceQueryInterface
{
    SOCKET socket_{};
    concurrencpp::result_promise<SQResponseInfo<SQ_PLAYERS>> response_promise_{};

public:
    explicit SourceQueryPlayers(SOCKET socket);

    concurrencpp::result<SQResponseInfo<SQ_PLAYERS>> SendPlayersQuery(netadr_t addr);

    void Send(netadr_t addr) override;
    bool TryResolve(netadr_t from_addr, ByteBuffer &buffer, char type, uint32_t ping_ms) override;
    void Resolve(SQErrorCode error_code, netadr_t from_addr) override;
    void ChallengeReceived(netadr_t from_addr, int challenge, uint32_t ping_ms) override;
};
