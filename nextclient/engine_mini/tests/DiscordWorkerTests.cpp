#ifndef _WIN32

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "service/discord/DiscordWorker.h"

namespace
{
    void SendFrame(int fd, uint32_t opcode, const std::string& payload)
    {
        std::string frame(8 + payload.size(), '\0');
        auto size = static_cast<uint32_t>(payload.size());
        for (int i = 0; i < 4; i++)
        {
            frame[i] = static_cast<char>((opcode >> (8 * i)) & 0xFF);
            frame[4 + i] = static_cast<char>((size >> (8 * i)) & 0xFF);
        }
        frame.replace(8, payload.size(), payload);

        // The worker may already have hung up on us; that is part of the test, not a failure
        send(fd, frame.data(), frame.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
    }

    bool HasLog(const std::vector<DiscordEvent>& events, const std::string& text)
    {
        for (const DiscordEvent& event : events)
        {
            if (event.type == DiscordEvent::Type::Log && event.text == text)
            {
                return true;
            }
        }
        return false;
    }
}

// A Discord that accepts the connection and then stops reading must not stall the game thread or shutdown
TEST(DiscordWorkerTest, StaysResponsiveWhenDiscordStopsReading)
{
    using namespace std::chrono;

    char dir[] = "/tmp/nextclient-discord-XXXXXX";
    ASSERT_NE(mkdtemp(dir), nullptr);
    std::string path = std::string(dir) + "/discord-ipc-0";

    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(server, 0);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
    ASSERT_EQ(bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    ASSERT_EQ(listen(server, 1), 0);

    const char* old_dir = getenv("XDG_RUNTIME_DIR");
    std::string saved_dir = old_dir != nullptr ? old_dir : "";
    setenv("XDG_RUNTIME_DIR", dir, 1);

    {
        DiscordWorker worker("123");
        worker.Start();

        pollfd pfd{server, POLLIN, 0};
        ASSERT_EQ(poll(&pfd, 1, 2000), 1);
        int client = accept(server, nullptr, nullptr);
        ASSERT_GE(client, 0);

        SendFrame(client, 1, R"({"cmd":"DISPATCH","evt":"READY","data":{"v":1},"nonce":null})");

        // Every ping asks for an equally big pong, and nobody reads the pongs
        std::string ping = R"({"p":")" + std::string(60000, 'x') + R"("})";
        for (int i = 0; i < 40; i++)
        {
            SendFrame(client, 3, ping);
        }

        std::vector<DiscordEvent> events;
        auto deadline = steady_clock::now() + seconds(3);

        while (!HasLog(events, "pong failed") && steady_clock::now() < deadline)
        {
            auto before = steady_clock::now();
            worker.SetActivity(R"({"details":"A"})", R"({"details":"fallback"})");
            std::vector<DiscordEvent> taken = worker.TakeEvents();
            EXPECT_LT(steady_clock::now() - before, milliseconds(50));

            events.insert(events.end(), taken.begin(), taken.end());
            std::this_thread::sleep_for(milliseconds(10));
        }

        EXPECT_TRUE(HasLog(events, "pong failed"));

        auto before_stop = steady_clock::now();
        worker.Stop();
        EXPECT_LT(steady_clock::now() - before_stop, seconds(1));

        close(client);
    }

    if (old_dir != nullptr)
    {
        setenv("XDG_RUNTIME_DIR", saved_dir.c_str(), 1);
    }
    else
    {
        unsetenv("XDG_RUNTIME_DIR");
    }

    close(server);
    unlink(path.c_str());
    rmdir(dir);
}

#endif
