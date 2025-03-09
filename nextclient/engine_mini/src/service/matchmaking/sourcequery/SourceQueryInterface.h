#pragma once
#include <data_types/ByteBuffer.h>
#include <netadr.h>

#include "SQErrorCode.h"

class SourceQueryInterface
{
public:
    virtual ~SourceQueryInterface() = default;

    virtual void Send(netadr_t addr) = 0;
    virtual bool TryResolve(netadr_t from_addr, ByteBuffer& buffer, char type, uint32_t ping_ms) = 0;
    virtual void Resolve(SQErrorCode error_code, netadr_t from_addr) = 0;
    virtual void ChallengeReceived(netadr_t from_addr, int challenge, uint32_t ping_ms) = 0;
};
