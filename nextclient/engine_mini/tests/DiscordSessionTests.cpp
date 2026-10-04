#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <tao/json.hpp>

#include "service/discord/DiscordSession.h"

namespace
{
    constexpr const char* kReady = R"({"cmd":"DISPATCH","evt":"READY","data":{"v":1},"nonce":null})";
    constexpr const char* kActivityA = R"({"details":"A"})";
    constexpr const char* kActivityB = R"({"details":"B"})";
    constexpr const char* kFallback = R"({"details":"fallback"})";

    class FakeTransport : public DiscordTransportInterface
    {
    public:
        bool accept = true;
        bool open = false;
        int opens = 0;
        std::vector<std::pair<DiscordOpcode, std::string>> written;
        std::deque<std::pair<DiscordOpcode, std::string>> incoming;

        bool Open() override
        {
            opens++;
            open = accept;
            return open;
        }

        void Close() override
        {
            open = false;
            incoming.clear();
        }

        bool is_open() const override
        {
            return open;
        }

        bool Write(DiscordOpcode opcode, std::string_view payload) override
        {
            if (!open)
            {
                return false;
            }
            written.emplace_back(opcode, std::string(payload));
            return true;
        }

        bool Poll(DiscordOpcode& opcode, std::string& payload) override
        {
            if (!open || incoming.empty())
            {
                return false;
            }
            opcode = incoming.front().first;
            payload = std::move(incoming.front().second);
            incoming.pop_front();
            return true;
        }

        bool Wait(int) override
        {
            return false;
        }

        void Receive(std::string json)
        {
            incoming.emplace_back(DiscordOpcode::Frame, std::move(json));
        }
    };

    class DiscordSessionTest : public testing::Test
    {
    protected:
        FakeTransport transport;
        DiscordSession session{transport, "123", 42};

        void Connect(double now)
        {
            session.Tick(now);
            transport.Receive(kReady);
            session.Tick(now);
        }

        std::vector<tao::json::value> Commands(const char* cmd) const
        {
            std::vector<tao::json::value> commands;

            for (const auto& [opcode, payload] : transport.written)
            {
                if (opcode != DiscordOpcode::Frame)
                {
                    continue;
                }

                tao::json::value json = tao::json::from_string(payload);
                if (json.at("cmd").get_string() == cmd)
                {
                    commands.push_back(std::move(json));
                }
            }

            return commands;
        }

        std::vector<tao::json::value> SentActivities() const
        {
            std::vector<tao::json::value> activities;
            for (const tao::json::value& command : Commands("SET_ACTIVITY"))
            {
                activities.push_back(command.at("args").at("activity"));
            }
            return activities;
        }

        std::string LastActivityNonce() const
        {
            return Commands("SET_ACTIVITY").back().at("nonce").get_string();
        }

        void Answer(const std::string& nonce, bool error)
        {
            tao::json::value answer = {
                {"cmd", "SET_ACTIVITY"},
                {"nonce", nonce},
                {"data", tao::json::empty_object}
            };
            if (error)
            {
                answer["evt"] = "ERROR";
                answer["data"] = {{"code", 4000}, {"message", "bad"}};
            }
            transport.Receive(tao::json::to_string(answer));
        }

        bool HasJoin()
        {
            for (const DiscordEvent& event : session.TakeEvents())
            {
                if (event.type == DiscordEvent::Type::Join)
                {
                    return true;
                }
            }
            return false;
        }
    };
}

TEST_F(DiscordSessionTest, SendsHandshakeWithAppId)
{
    session.Tick(0);

    ASSERT_EQ(transport.written.size(), 1u);
    EXPECT_EQ(transport.written[0].first, DiscordOpcode::Handshake);

    tao::json::value handshake = tao::json::from_string(transport.written[0].second);
    EXPECT_EQ(handshake.at("v").as<int>(), 1);
    EXPECT_EQ(handshake.at("client_id").get_string(), "123");
}

TEST_F(DiscordSessionTest, SendsActivityOnlyAfterReady)
{
    session.SetActivity(kActivityA, kFallback);
    session.Tick(0);
    session.Tick(1);

    EXPECT_FALSE(session.is_ready());
    EXPECT_TRUE(SentActivities().empty());

    transport.Receive(kReady);
    session.Tick(2);

    EXPECT_TRUE(session.is_ready());
    EXPECT_EQ(Commands("SUBSCRIBE").size(), 1u);
    ASSERT_EQ(SentActivities().size(), 1u);
    EXPECT_EQ(SentActivities()[0].at("details").get_string(), "A");
    EXPECT_EQ(Commands("SET_ACTIVITY")[0].at("args").at("pid").as<int>(), 42);
}

TEST_F(DiscordSessionTest, ClosesWhenTheFirstFrameIsNotReady)
{
    session.Tick(0);
    transport.Receive(R"({"cmd":"DISPATCH","evt":"ERROR","data":{"code":4000}})");
    session.Tick(0);

    EXPECT_FALSE(transport.open);
    EXPECT_FALSE(session.is_ready());
}

TEST_F(DiscordSessionTest, ClosesWhenTheFirstFrameIsNotJson)
{
    session.Tick(0);
    transport.Receive("{not json");
    session.Tick(0);

    EXPECT_FALSE(transport.open);
}

TEST_F(DiscordSessionTest, GivesUpOnReadyAndReconnectsLater)
{
    session.Tick(0);
    session.Tick(9);
    EXPECT_TRUE(transport.open);

    session.Tick(10.5);
    EXPECT_FALSE(transport.open);

    session.Tick(14);
    EXPECT_EQ(transport.opens, 1);

    session.Tick(15.5);
    EXPECT_EQ(transport.opens, 2);
    EXPECT_TRUE(transport.open);
}

TEST_F(DiscordSessionTest, RetriesRejectedActivityWhileTheGameStateStaysTheSame)
{
    session.SetActivity(kActivityA, kFallback);
    Connect(0);
    Answer(LastActivityNonce(), true);

    session.Tick(1);
    EXPECT_EQ(SentActivities().size(), 1u);

    session.Tick(6);
    ASSERT_EQ(SentActivities().size(), 2u);
    EXPECT_EQ(SentActivities()[1].at("details").get_string(), "A");

    Answer(LastActivityNonce(), false);
    session.Tick(7);
    session.Tick(60);
    EXPECT_EQ(SentActivities().size(), 2u);
}

TEST_F(DiscordSessionTest, LateAnswerToAnOlderCommandIsIgnored)
{
    session.SetActivity(kActivityA, kFallback);
    Connect(0);
    std::string first_nonce = LastActivityNonce();

    // No answer within the timeout, so it goes out again with a new nonce
    session.Tick(10.5);
    session.Tick(16);
    ASSERT_EQ(SentActivities().size(), 2u);
    std::string second_nonce = LastActivityNonce();
    EXPECT_NE(first_nonce, second_nonce);

    Answer(first_nonce, true);
    Answer(second_nonce, false);
    session.Tick(17);
    session.Tick(60);

    EXPECT_EQ(SentActivities().size(), 2u);
}

TEST_F(DiscordSessionTest, FallsBackAndThenStopsWhenDiscordKeepsRejecting)
{
    session.SetActivity(kActivityA, kFallback);
    Connect(0);

    double now = 0;
    for (int i = 0; i < 6; i++)
    {
        Answer(LastActivityNonce(), true);
        size_t sent = SentActivities().size();

        while (SentActivities().size() == sent && now < 100)
        {
            now += 1;
            session.Tick(now);
        }
    }

    std::vector<tao::json::value> sent = SentActivities();
    ASSERT_EQ(sent.size(), 6u);
    EXPECT_EQ(sent[2].at("details").get_string(), "A");
    EXPECT_EQ(sent[3].at("details").get_string(), "fallback");
    EXPECT_EQ(sent[5].at("details").get_string(), "fallback");

    for (int i = 0; i < 100; i++)
    {
        session.Tick(now + i);
    }
    EXPECT_EQ(SentActivities().size(), 6u);

    // A new game state is worth another try
    session.SetActivity(kActivityB, kFallback);
    session.Tick(now + 200);
    ASSERT_EQ(SentActivities().size(), 7u);
    EXPECT_EQ(SentActivities()[6].at("details").get_string(), "B");
}

TEST_F(DiscordSessionTest, KeepsTheRateLimitBetweenActivities)
{
    session.SetActivity(kActivityA, kFallback);
    Connect(0);
    Answer(LastActivityNonce(), false);
    session.Tick(1);

    session.SetActivity(kActivityB, kFallback);
    session.Tick(2);
    session.Tick(4.9);
    EXPECT_EQ(SentActivities().size(), 1u);

    session.Tick(5.1);
    EXPECT_EQ(SentActivities().size(), 2u);
}

TEST_F(DiscordSessionTest, ResendsTheActivityAfterReconnecting)
{
    session.SetActivity(kActivityA, kFallback);
    Connect(0);
    Answer(LastActivityNonce(), false);
    session.Tick(1);

    transport.Close();
    session.Tick(2);
    EXPECT_FALSE(session.is_ready());

    session.Tick(20);
    EXPECT_EQ(transport.opens, 2);
    transport.Receive(kReady);
    session.Tick(20);

    EXPECT_EQ(SentActivities().size(), 2u);
}

TEST_F(DiscordSessionTest, AnswersPingAndHonoursClose)
{
    Connect(0);

    transport.incoming.emplace_back(DiscordOpcode::Ping, "{\"x\":1}");
    session.Tick(1);
    ASSERT_FALSE(transport.written.empty());
    EXPECT_EQ(transport.written.back().first, DiscordOpcode::Pong);
    EXPECT_EQ(transport.written.back().second, "{\"x\":1}");

    transport.incoming.emplace_back(DiscordOpcode::Close, "{\"code\":1000}");
    session.Tick(2);
    EXPECT_FALSE(transport.open);
    EXPECT_FALSE(session.is_ready());
}

TEST_F(DiscordSessionTest, PassesOnlyCheckedJoinAddresses)
{
    Connect(0);
    session.TakeEvents();

    transport.Receive(R"({"cmd":"DISPATCH","evt":"ACTIVITY_JOIN","data":{"secret":"1.2.3.4:27015;quit"}})");
    session.Tick(1);
    EXPECT_FALSE(HasJoin());

    transport.Receive(R"({"cmd":"DISPATCH","evt":"ACTIVITY_JOIN","data":{"secret":"1.2.3.4:27015"}})");
    session.Tick(2);
    std::vector<DiscordEvent> events = session.TakeEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, DiscordEvent::Type::Join);
    EXPECT_EQ(events[0].text, "1.2.3.4:27015");
}
