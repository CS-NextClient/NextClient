#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "DiscordTransportInterface.h"

class DiscordIpc : public DiscordTransportInterface
{
#ifdef _WIN32
    void* pipe_ = nullptr;
#else
    int fd_ = -1;
#endif

    // Bytes received so far; a message can arrive split across several reads
    std::string recv_buf_{};

    bool WriteBytes(const void* data, size_t size);
    bool ReceiveAvailable();
    bool TakeMessage(DiscordOpcode& opcode, std::string& payload);

public:
    DiscordIpc() = default;
    ~DiscordIpc() override;

    DiscordIpc(const DiscordIpc&) = delete;
    DiscordIpc& operator=(const DiscordIpc&) = delete;

    // Connects to the first discord-ipc-N endpoint the local Discord client listens on
    bool Open() override;
    void Close() override;
    bool is_open() const override;

    // Each message is an 8-byte header (opcode, payload size; both little-endian) followed by JSON
    bool Write(DiscordOpcode opcode, std::string_view payload) override;
    bool Poll(DiscordOpcode& opcode, std::string& payload) override;
    bool Wait(int timeout_ms) override;
};
