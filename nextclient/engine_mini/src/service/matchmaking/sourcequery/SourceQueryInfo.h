#pragma once
#include <taskcoro/TaskCoro.h>
#include <data_types/ByteBuffer.h>
#include <netadr.h>

#include "SourceQueryInterface.h"
#include "SQResponseInfo.h"
#include "source_query_types.h"

class SourceQueryInfo : public SourceQueryInterface
{
    SOCKET socket_{};
    concurrencpp::result_promise<SQResponseInfo<SQ_INFO>> response_promise_{};

public:
    explicit SourceQueryInfo(SOCKET socket);
    ~SourceQueryInfo() override = default;

    concurrencpp::result<SQResponseInfo<SQ_INFO>> SendInfoQuery(netadr_t addr);

    void Send(netadr_t addr) override;
    bool TryResolve(netadr_t from_addr, ByteBuffer &buffer, char type, uint32_t ping_ms) override;
    void Resolve(SQErrorCode error_code, netadr_t from_addr) override;
    void ChallengeReceived(netadr_t from_addr, int challenge, uint32_t ping_ms) override;

private:
    static SQ_INFO ParseInfo(ByteBuffer &buffer);
    static SQ_INFO ParseRulesGs(ByteBuffer &buffer);
};
