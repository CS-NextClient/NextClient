#include "DiscordSession.h"

#include <algorithm>
#include <utility>

#include <tao/json.hpp>

#include "DiscordValidation.h"

namespace
{
    constexpr double kReconnectDelay = 15.0;
    constexpr double kReadyTimeout = 10.0;
    constexpr double kResponseTimeout = 10.0;

    // Discord takes at most 5 SET_ACTIVITY per 20 seconds
    constexpr double kActivityInterval = 5.0;

    // Waits before the 2nd and 3rd attempt; after the 3rd failure the activity is given up
    constexpr double kRetryDelays[] = {5.0, 15.0};
    constexpr int kMaxAttempts = 3;

    // Given-up activities are kept until the game state changes, this many at most
    constexpr size_t kMaxGivenUp = 2;

    bool JsonFieldIs(const tao::json::value& json, const char* key, const char* expected)
    {
        const tao::json::value* field = json.find(key);
        return field != nullptr && field->is_string() && field->get_string() == expected;
    }
}

DiscordSession::DiscordSession(DiscordTransportInterface& transport, const std::string& app_id, int pid) :
    transport_(transport),
    pid_(pid)
{
    tao::json::value handshake = {
        {"v", 1},
        {"client_id", app_id}
    };
    handshake_ = tao::json::to_string(handshake);
}

void DiscordSession::SetActivity(std::string activity, std::string fallback)
{
    if (activity != desired_)
    {
        // A new game state gets its own attempts, but not sooner than the rate limit allows
        given_up_.clear();
        failures_ = 0;
        next_send_time_ = std::min(next_send_time_, last_send_time_ + kActivityInterval);
    }

    desired_ = std::move(activity);
    fallback_ = std::move(fallback);
}

void DiscordSession::Tick(double now)
{
    if (!transport_.is_open())
    {
        ResetConnectionState();

        if (now < next_connect_time_)
        {
            return;
        }

        next_connect_time_ = now + kReconnectDelay;

        if (!transport_.Open() || !transport_.Write(DiscordOpcode::Handshake, handshake_))
        {
            return;
        }

        ready_deadline_ = now + kReadyTimeout;
    }

    ProcessIncoming(now);

    if (!transport_.is_open())
    {
        ResetConnectionState();
        return;
    }

    if (!ready_)
    {
        if (now > ready_deadline_)
        {
            Log("no READY from Discord");
            transport_.Close();
            ResetConnectionState();
        }
        return;
    }

    if (pending_ && now > pending_->sent_at + kResponseTimeout)
    {
        Log("no answer to SET_ACTIVITY");
        std::string activity = std::move(pending_->activity);
        pending_.reset();
        OnActivityFailed(std::move(activity), now);
    }

    SendActivity(now);
}

std::vector<DiscordEvent> DiscordSession::TakeEvents()
{
    return std::exchange(events_, {});
}

bool DiscordSession::is_ready() const
{
    return ready_;
}

void DiscordSession::ResetConnectionState()
{
    ready_ = false;
    confirmed_.clear();
    pending_.reset();
    given_up_.clear();
    failures_ = 0;
    next_send_time_ = 0;
}

void DiscordSession::ProcessIncoming(double now)
{
    DiscordOpcode opcode{};
    std::string message;

    while (transport_.Poll(opcode, message))
    {
        switch (opcode)
        {
            case DiscordOpcode::Frame:
            {
                HandleFrame(message, now);
                break;
            }
            case DiscordOpcode::Ping:
            {
                if (!transport_.Write(DiscordOpcode::Pong, message))
                {
                    Log("pong failed");
                }
                break;
            }
            case DiscordOpcode::Close:
            {
                Log("close " + message);
                transport_.Close();
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

void DiscordSession::HandleFrame(const std::string& message, double now)
{
    tao::json::value json;

    try
    {
        json = tao::json::from_string(message);
    }
    catch (const std::exception& e)
    {
        Log(std::string("bad message: ") + e.what());

        if (!ready_)
        {
            transport_.Close();
        }
        return;
    }

    if (!ready_)
    {
        if (JsonFieldIs(json, "cmd", "DISPATCH") && JsonFieldIs(json, "evt", "READY"))
        {
            ready_ = true;
            Log("ready!");

            tao::json::value subscribe = {
                {"cmd", "SUBSCRIBE"},
                {"evt", "ACTIVITY_JOIN"},
                {"nonce", std::to_string(++nonce_)}
            };
            transport_.Write(DiscordOpcode::Frame, tao::json::to_string(subscribe));
        }
        else
        {
            Log("handshake failed " + message);
            transport_.Close();
        }
        return;
    }

    // Answers to older SET_ACTIVITY commands carry other nonces and fall through to the log below
    if (pending_ && JsonFieldIs(json, "nonce", pending_->nonce.c_str()))
    {
        std::string activity = std::move(pending_->activity);
        pending_.reset();

        if (JsonFieldIs(json, "evt", "ERROR"))
        {
            Log("activity rejected " + message);
            OnActivityFailed(std::move(activity), now);
        }
        else
        {
            confirmed_ = std::move(activity);
            failures_ = 0;
        }
        return;
    }

    if (JsonFieldIs(json, "evt", "ERROR"))
    {
        Log("error " + message);
    }
    else if (JsonFieldIs(json, "cmd", "DISPATCH") && JsonFieldIs(json, "evt", "ACTIVITY_JOIN"))
    {
        const tao::json::value* data = json.find("data");
        const tao::json::value* secret = data != nullptr && data->is_object() ? data->find("secret") : nullptr;

        if (secret == nullptr || !secret->is_string() || !Discord_IsSafeJoinAddress(secret->get_string().c_str()))
        {
            Log("rejected join address");
            return;
        }

        events_.push_back({DiscordEvent::Type::Join, secret->get_string()});
    }
}

void DiscordSession::OnActivityFailed(std::string activity, double now)
{
    // The game state moved on while this one was waiting for an answer
    if (activity != desired_ && activity != fallback_)
    {
        return;
    }

    failures_++;

    if (failures_ < kMaxAttempts)
    {
        next_send_time_ = std::max(next_send_time_, now + kRetryDelays[failures_ - 1]);
        return;
    }

    failures_ = 0;

    if (given_up_.size() < kMaxGivenUp)
    {
        given_up_.push_back(std::move(activity));
    }
}

void DiscordSession::SendActivity(double now)
{
    if (pending_ || now < next_send_time_)
    {
        return;
    }

    const std::string* activity = &desired_;
    if (HasGivenUp(*activity))
    {
        activity = &fallback_;
    }

    if (activity->empty() || *activity == confirmed_ || HasGivenUp(*activity))
    {
        return;
    }

    tao::json::value parsed;

    try
    {
        parsed = tao::json::from_string(*activity);
    }
    catch (const std::exception& e)
    {
        Log(std::string("bad activity: ") + e.what());
        given_up_.push_back(*activity);
        return;
    }

    std::string nonce = std::to_string(++nonce_);

    tao::json::value command = {
        {"cmd", "SET_ACTIVITY"},
        {"nonce", nonce},
        {"args", {{"pid", pid_}, {"activity", std::move(parsed)}}}
    };

    if (!transport_.Write(DiscordOpcode::Frame, tao::json::to_string(command)))
    {
        Log("frame failed");
        return;
    }

    pending_ = PendingActivity{std::move(nonce), *activity, now};
    last_send_time_ = now;
    next_send_time_ = now + kActivityInterval;
}

bool DiscordSession::HasGivenUp(const std::string& activity) const
{
    return std::find(given_up_.begin(), given_up_.end(), activity) != given_up_.end();
}

void DiscordSession::Log(std::string text)
{
    events_.push_back({DiscordEvent::Type::Log, std::move(text)});
}
