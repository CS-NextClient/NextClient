#pragma once

#include <optional>
#include <string>
#include <vector>

#include "DiscordTransportInterface.h"

struct DiscordEvent
{
    enum class Type
    {
        Log,
        Join,
    };

    Type type{};

    // The message for Log, a checked ip:port for Join
    std::string text{};
};

// The Discord RPC protocol over a transport: handshake, READY, activity and join events.
// Knows nothing about threads or the engine, so tests can drive it with a fake transport and clock.
class DiscordSession
{
    struct PendingActivity
    {
        std::string nonce{};
        std::string activity{};
        double sent_at{};
    };

    DiscordTransportInterface& transport_;
    std::string handshake_{};
    int pid_{};

    bool ready_{};
    double next_connect_time_{};
    double ready_deadline_{};
    unsigned long long nonce_{};

    std::string desired_{};
    std::string fallback_{};
    std::string confirmed_{};
    std::optional<PendingActivity> pending_{};
    std::vector<std::string> given_up_{};
    int failures_{};
    double last_send_time_{};
    double next_send_time_{};

    std::vector<DiscordEvent> events_{};

    void ResetConnectionState();
    void ProcessIncoming(double now);
    void HandleFrame(const std::string& message, double now);
    void HandleJoin(const std::string& message);
    void OnActivityFailed(std::string activity, double now);
    void SendActivity(double now);
    bool HasGivenUp(const std::string& activity) const;
    void Log(std::string text);

public:
    DiscordSession(DiscordTransportInterface& transport, const std::string& app_id, int pid);

    // Both are JSON objects. The fallback is a plain activity that goes out instead
    // once Discord keeps rejecting the main one.
    void SetActivity(std::string activity, std::string fallback);

    // Connects, answers Discord and sends the activity when it changed; now is in seconds on a steady clock
    void Tick(double now);

    std::vector<DiscordEvent> TakeEvents();

    bool is_ready() const;
};
