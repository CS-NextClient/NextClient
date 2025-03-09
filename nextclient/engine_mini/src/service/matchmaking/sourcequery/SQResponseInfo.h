#pragma once
#include <netadr.h>
#include "SQErrorCode.h"

template<class T>
struct SQResponseInfo
{
    T value{};
    SQErrorCode error_code{};
    netadr_t address{};
    uint32_t ping_ms{};

    explicit SQResponseInfo()
    { }

    explicit SQResponseInfo(T&& value, netadr_t address, uint32_t ping_ms) :
        value(std::move(value)),
        error_code(SQErrorCode::Ok),
        address(address),
        ping_ms(ping_ms)
    { }

    explicit SQResponseInfo(SQErrorCode error_code, netadr_t address, uint32_t ping_ms) :
        error_code(error_code),
        address(address),
        ping_ms(ping_ms)
    { }

    SQResponseInfo(SQResponseInfo&& obj) noexcept :
        value(std::move(obj.value)),
        error_code(obj.error_code),
        address(obj.address),
        ping_ms(obj.ping_ms)
    { }

    SQResponseInfo(const SQResponseInfo& obj) :
        value(obj.value),
        error_code(obj.error_code),
        address(obj.address),
        ping_ms(obj.ping_ms)
    { }
};
