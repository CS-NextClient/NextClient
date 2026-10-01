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

    bool WriteBytes(const void* data, size_t size);
    bool ReadBytes(void* data, size_t size);

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
    // Both block until the whole message is through and close the connection on failure.
    bool Write(DiscordOpcode opcode, std::string_view payload);
    bool Read(DiscordOpcode& opcode, std::string& payload);
};
