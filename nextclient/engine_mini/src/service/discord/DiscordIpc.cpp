#include "DiscordIpc.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

// Discord opens discord-ipc-0, and takes the next free number when another client already holds it
static constexpr int kMaxPipeNumber = 10;

// Discord's own messages are a few KB; anything far bigger means the stream is out of sync
static constexpr uint32_t kMaxPayloadSize = 64 * 1024;

static constexpr size_t kHeaderSize = 8;

// Our messages are tiny, so a full send buffer means Discord stopped reading
static constexpr int kWriteTimeoutMs = 100;

static void WriteUint32Le(uint8_t* out, uint32_t value)
{
    out[0] = value & 0xFF;
    out[1] = (value >> 8) & 0xFF;
    out[2] = (value >> 16) & 0xFF;
    out[3] = (value >> 24) & 0xFF;
}

static uint32_t ReadUint32Le(const uint8_t* in)
{
    return in[0] | (in[1] << 8) | (in[2] << 16) | (static_cast<uint32_t>(in[3]) << 24);
}

DiscordIpc::~DiscordIpc()
{
    Close();
}

bool DiscordIpc::Write(DiscordOpcode opcode, std::string_view payload)
{
    uint8_t header[kHeaderSize];
    WriteUint32Le(header, static_cast<uint32_t>(opcode));
    WriteUint32Le(header + 4, static_cast<uint32_t>(payload.size()));

    return WriteBytes(header, sizeof(header)) && WriteBytes(payload.data(), payload.size());
}

bool DiscordIpc::TakeMessage(DiscordOpcode& opcode, std::string& payload)
{
    if (recv_buf_.size() < kHeaderSize)
        return false;

    const auto* header = reinterpret_cast<const uint8_t*>(recv_buf_.data());
    uint32_t size = ReadUint32Le(header + 4);

    if (size > kMaxPayloadSize)
    {
        Close();
        return false;
    }

    if (recv_buf_.size() < kHeaderSize + size)
        return false;

    opcode = static_cast<DiscordOpcode>(ReadUint32Le(header));
    payload.assign(recv_buf_, kHeaderSize, size);
    recv_buf_.erase(0, kHeaderSize + size);

    return true;
}

bool DiscordIpc::Poll(DiscordOpcode& opcode, std::string& payload)
{
    if (!is_open())
        return false;

    // An earlier read may have brought in more than one message
    if (TakeMessage(opcode, payload))
        return true;

    return ReceiveAvailable() && TakeMessage(opcode, payload);
}

bool DiscordIpc::Read(DiscordOpcode& opcode, std::string& payload, int timeout_ms)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (!Poll(opcode, payload))
    {
        if (!is_open())
            return false;

        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0 || !WaitReadable(static_cast<int>(left.count())))
            return false;
    }

    return true;
}

#ifdef _WIN32

bool DiscordIpc::Open()
{
    Close();

    for (int i = 0; i < kMaxPipeNumber; i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "\\\\?\\pipe\\discord-ipc-%d", i);

        HANDLE pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE)
        {
            pipe_ = pipe;
            return true;
        }
    }

    return false;
}

void DiscordIpc::Close()
{
    if (pipe_)
    {
        CloseHandle(pipe_);
        pipe_ = nullptr;
    }

    recv_buf_.clear();
}

bool DiscordIpc::is_open() const
{
    return pipe_ != nullptr;
}

bool DiscordIpc::WriteBytes(const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(data);

    while (size > 0)
    {
        DWORD written = 0;
        if (!pipe_ || !WriteFile(pipe_, bytes, static_cast<DWORD>(size), &written, nullptr))
        {
            Close();
            return false;
        }

        bytes += written;
        size -= written;
    }

    return true;
}

bool DiscordIpc::ReceiveAvailable()
{
    // ReadFile on a pipe blocks until data arrives, so only ask for what is already there
    DWORD available = 0;
    if (!PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr))
    {
        Close();
        return false;
    }

    while (available > 0)
    {
        char chunk[4096];
        DWORD read = 0;

        if (!ReadFile(pipe_, chunk, available < sizeof(chunk) ? available : static_cast<DWORD>(sizeof(chunk)), &read, nullptr) || read == 0)
        {
            Close();
            return false;
        }

        recv_buf_.append(chunk, read);
        available -= read;
    }

    return true;
}

bool DiscordIpc::WaitReadable(int timeout_ms)
{
    // Pipes opened without FILE_FLAG_OVERLAPPED have nothing to wait on, so poll the byte count
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    do
    {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe_, nullptr, 0, nullptr, &available, nullptr))
        {
            Close();
            return false;
        }

        if (available > 0)
            return true;

        Sleep(1);
    } while (std::chrono::steady_clock::now() < deadline);

    return false;
}

#else

static const char* GetSocketDirectory()
{
    static const char* const kVars[] = { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" };

    for (const char* var : kVars)
    {
        const char* dir = getenv(var);
        if (dir && *dir)
            return dir;
    }

    return "/tmp";
}

bool DiscordIpc::Open()
{
    Close();

    const char* dir = GetSocketDirectory();

    for (int i = 0; i < kMaxPipeNumber; i++)
    {
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/discord-ipc-%d", dir, i);

        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd == -1)
            return false;

        // A local connect finishes at once, so only switch to non-blocking once it is through
        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == 0)
        {
            fd_ = fd;
            return true;
        }

        close(fd);
    }

    return false;
}

void DiscordIpc::Close()
{
    if (fd_ != -1)
    {
        close(fd_);
        fd_ = -1;
    }

    recv_buf_.clear();
}

bool DiscordIpc::is_open() const
{
    return fd_ != -1;
}

bool DiscordIpc::WriteBytes(const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(data);

    while (size > 0)
    {
        // MSG_NOSIGNAL: a write after Discord quits would otherwise raise SIGPIPE and kill the game
        ssize_t written = fd_ == -1 ? -1 : send(fd_, bytes, size, MSG_NOSIGNAL);
        if (written > 0)
        {
            bytes += written;
            size -= written;
            continue;
        }

        if (written == -1 && errno == EINTR)
            continue;

        if (written == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            pollfd pfd{ fd_, POLLOUT, 0 };
            if (poll(&pfd, 1, kWriteTimeoutMs) > 0)
                continue;
        }

        Close();
        return false;
    }

    return true;
}

bool DiscordIpc::ReceiveAvailable()
{
    for (;;)
    {
        char chunk[4096];
        ssize_t read = recv(fd_, chunk, sizeof(chunk), 0);

        if (read > 0)
        {
            recv_buf_.append(chunk, read);
            continue;
        }

        if (read == -1 && errno == EINTR)
            continue;

        // Drained everything there was; the connection is still fine
        if (read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;

        // 0 means Discord closed its end
        Close();
        return false;
    }
}

bool DiscordIpc::WaitReadable(int timeout_ms)
{
    pollfd pfd{ fd_, POLLIN, 0 };
    return poll(&pfd, 1, timeout_ms) > 0;
}

#endif
