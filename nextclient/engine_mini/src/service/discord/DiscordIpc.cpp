#include "DiscordIpc.h"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

// Discord opens discord-ipc-0, and takes the next free number when another client already holds it
static constexpr int kMaxPipeNumber = 10;

// Discord's own messages are a few KB; anything far bigger means the stream is out of sync
static constexpr uint32_t kMaxPayloadSize = 64 * 1024;

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
    uint8_t header[8];
    WriteUint32Le(header, static_cast<uint32_t>(opcode));
    WriteUint32Le(header + 4, static_cast<uint32_t>(payload.size()));

    return WriteBytes(header, sizeof(header)) && WriteBytes(payload.data(), payload.size());
}

bool DiscordIpc::Read(DiscordOpcode& opcode, std::string& payload)
{
    uint8_t header[8];
    if (!ReadBytes(header, sizeof(header)))
        return false;

    uint32_t size = ReadUint32Le(header + 4);
    if (size > kMaxPayloadSize)
    {
        Close();
        return false;
    }

    opcode = static_cast<DiscordOpcode>(ReadUint32Le(header));
    payload.resize(size);

    return ReadBytes(payload.data(), size);
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

bool DiscordIpc::ReadBytes(void* data, size_t size)
{
    auto* bytes = static_cast<uint8_t*>(data);

    while (size > 0)
    {
        DWORD read = 0;
        if (!pipe_ || !ReadFile(pipe_, bytes, static_cast<DWORD>(size), &read, nullptr) || read == 0)
        {
            Close();
            return false;
        }

        bytes += read;
        size -= read;
    }

    return true;
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

        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
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
        if (written <= 0)
        {
            if (written == -1 && errno == EINTR)
                continue;

            Close();
            return false;
        }

        bytes += written;
        size -= written;
    }

    return true;
}

bool DiscordIpc::ReadBytes(void* data, size_t size)
{
    auto* bytes = static_cast<uint8_t*>(data);

    while (size > 0)
    {
        ssize_t read = fd_ == -1 ? -1 : recv(fd_, bytes, size, 0);
        if (read <= 0)
        {
            if (read == -1 && errno == EINTR)
                continue;

            Close();
            return false;
        }

        bytes += read;
        size -= read;
    }

    return true;
}

#endif
