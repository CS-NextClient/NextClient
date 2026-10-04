#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class DiscordOpcode : uint32_t
{
    Handshake = 0,
    Frame = 1,
    Close = 2,
    Ping = 3,
    Pong = 4,
};

class DiscordTransportInterface
{
public:
    virtual ~DiscordTransportInterface() = default;

    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool is_open() const = 0;

    // Sends one whole frame; closes the connection when it breaks, so check is_open() after a false
    virtual bool Write(DiscordOpcode opcode, std::string_view payload) = 0;

    // Never blocks: false when no complete message has arrived yet
    virtual bool Poll(DiscordOpcode& opcode, std::string& payload) = 0;

    // Waits up to timeout_ms for incoming data
    virtual bool Wait(int timeout_ms) = 0;
};
