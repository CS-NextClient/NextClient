#pragma once

#include <cstddef>
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

class DiscordIpc
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
    bool WaitReadable(int timeout_ms);
    bool TakeMessage(DiscordOpcode& opcode, std::string& payload);

public:
    DiscordIpc() = default;
    ~DiscordIpc();

    DiscordIpc(const DiscordIpc&) = delete;
    DiscordIpc& operator=(const DiscordIpc&) = delete;

    // Connects to the first discord-ipc-N endpoint the local Discord client listens on
    bool Open();
    void Close();
    bool is_open() const;

    // Each message is an 8-byte header (opcode, payload size; both little-endian) followed by JSON.
    // All of them close the connection when it breaks, so check is_open() after a false.
    bool Write(DiscordOpcode opcode, std::string_view payload);

    // Never blocks: false when no complete message has arrived yet
    bool Poll(DiscordOpcode& opcode, std::string& payload);

    // Waits for the next message up to timeout_ms
    bool Read(DiscordOpcode& opcode, std::string& payload, int timeout_ms);
};
