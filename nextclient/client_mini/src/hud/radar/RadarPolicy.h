#pragma once

#include <array>
#include <cstdint>

#include <ncl_math/vec3.h>

#include "RadarLimits.h"

enum class BombIntent : std::uint8_t
{
    Unknown,
    Bomb,
    Marker,
};

enum class HostageIntent : std::uint8_t
{
    Unknown,
    Hostage,
    Marker,
};

enum class MessageHint : std::uint8_t
{
    None,
    Stock,
    Marker,
};

class RadarPolicy
{
public:
    using LogFn = void (*)(const char* message);

    // ReGameDLL resends an ally position at most once a second, and only after it moved 64 units.
    static constexpr float kStaleTime = 3.0f;
    // Lag budget in seconds of running, so a raised sv_maxspeed does not trip the check.
    static constexpr float kStaleSpeedSeconds = 2.0f;
    static constexpr float kMinStaleDist = 320.0f;
    // Without this a standing ally holding last round's position reads as withheld through freezetime.
    static constexpr float kStaleMoveDist = 128.0f;

    // Past the coordinate quantisation: a planted bomb cannot move at all.
    static constexpr float kPlantedMoveDist = 64.0f;
    // A dropped bomb only moves while it is in the air, which the once-a-second report catches in one
    // or two messages however far the throw carries it; a marker that follows a player keeps moving.
    static constexpr int kMoveMessages = 3; // messages that moved, not messages in total
    static constexpr float kMoveStepDist = 1.0f; // a shorter step repeats the last spot
    static constexpr float kMoveDist = 200.0f;
    static constexpr float kMoveWindow = 3.0f;

    // BombPickup / HostageK may never arrive for a re-purposed marker, so it is hidden
    // once the updates stop instead of hanging at the last position.
    static constexpr float kMarkerTimeout = 8.0f;

    // CHostage sends at most one position per second (m_flNextRadarTime); the allowance
    // is wide enough that a burst arriving after a lag spike does not trip it.
    static constexpr float kHostageRateWindow = 5.0f;
    static constexpr int kHostageRateLimit = 10;

    static constexpr int kMaxHostages = 256; // the message carries the index in one byte

    struct BombObservation
    {
        std::uint32_t seq; // bumped by RadarBroadcast on every bomb message
        bool active;
        ncl_math::Vector3 pos;
        bool planted;
        float message_time;
    };

    struct HostageObservation
    {
        int index; // the server's hostage index, 1..N on a stock map
        std::uint32_t seq;
        float message_time;
    };

private:
    enum class AllyReason : std::uint8_t
    {
        None,
        Broadcast,
        NoBroadcast,
        Zeroed,
        Stale,
        ForcedCvar,
        ForcedServer,
    };

    enum class BombReason : std::uint8_t
    {
        None,
        Sites,
        NoSites,
        PlantedMoved,
        Moved,
        Hint,
    };

    enum class HostageReason : std::uint8_t
    {
        None,
        MapHostages,
        NoMapHostages,
        IndexOutOfRange,
        TooFast,
        Hint,
    };

    enum class HostageRevealReason : std::uint8_t
    {
        None,
        Broadcast,
        NoBroadcast,
    };

    struct Ally
    {
        ncl_math::Vector3 broadcast;
        float drift_since = -1.0f; // -1 while the live position agrees with the broadcast
        ncl_math::Vector2 drift_start_pos;
        bool ever_broadcast{};
        bool revealed{};
        bool decided{};
        AllyReason reason = AllyReason::None;
    };

    struct BombTrack
    {
        ncl_math::Vector3 pos;
        bool planted{};
        int messages{};
        int moving_messages{};
        float moved{};
        float first_time{};
        float last_time{};
    };

    struct HostageTrack
    {
        std::uint32_t seq{};
        float window_start{};
        int window_count{};
        bool marker_visible{};
    };

    std::array<Ally, kRadarMaxPlayers + 1> allies_{};
    std::array<HostageTrack, kMaxHostages> hostages_{};
    BombTrack track_;

    LogFn log_{};
    bool logging_{};

    float time_{};
    float stale_dist_ = kMinStaleDist;
    bool applicable_{}; // false while spectating or teamless: nothing to gate

    bool force_gating_cvar_{};
    bool force_gating_server_{};

    int site_count_{};
    MessageHint bomb_hint_ = MessageHint::None;
    BombIntent bomb_intent_ = BombIntent::Unknown;
    BombReason bomb_reason_ = BombReason::None;
    std::uint32_t bomb_seq_{};
    bool marker_visible_{};

    int map_hostage_count_{};
    MessageHint hostage_hint_ = MessageHint::None;
    HostageIntent hostage_intent_ = HostageIntent::Unknown;
    HostageReason hostage_reason_ = HostageReason::None;
    bool hostages_revealed_ = true;
    HostageRevealReason hostage_reveal_reason_ = HostageRevealReason::None;

public:
    void SetLogger(LogFn fn);
    void SetLogging(bool enabled);

    void OnLevelChange();

    void SetMapObjectives(int bomb_sites, int hostages);
    void SetBombHint(MessageHint hint); // serverinfo nc_bombmsg
    void SetHostageHint(MessageHint hint); // serverinfo nc_hostagemsg
    void SetServerFfaHint(bool ffa); // serverinfo nc_ffa; only ever tightens
    void SetForceAllyGating(bool on); // hud_radar_allies 0

    void BeginFrame(float time, float run_speed, bool applicable);

    // The local player is excluded; broadcast is the ally's g_PlayerExtraInfo origin,
    // and live_pos is only meaningful while the ally is in the PVS.
    void ObserveAlly(int index, bool in_pvs, ncl_math::Vector2 live_pos, const ncl_math::Vector3& broadcast);

    void ObserveBomb(const BombObservation& obs, float now);

    // HostagePos reaches living Counter-Terrorists only; ct_side false leaves the answer
    // at "revealed" and logs nothing.
    void ObserveHostageBroadcast(bool ct_side, int active_count);

    void ObserveHostage(const HostageObservation& obs, float now);

    bool IsAllyRevealed(int index) const;

    BombIntent get_bomb_intent() const
    {
        return bomb_intent_;
    }

    // Only meaningful for BombIntent::Marker.
    bool is_marker_visible() const
    {
        return marker_visible_;
    }

    HostageIntent get_hostage_intent() const
    {
        return hostage_intent_;
    }

    bool are_hostages_revealed() const
    {
        return hostages_revealed_;
    }

    bool IsHostageMarkerVisible(int index) const;

private:
    void Log(const char* format, ...) const;
    void LogSnapshot() const;

    void DecideAlly(int index, bool revealed, AllyReason reason, float detail = 0.0f);
    void SetBombIntent(BombIntent intent, BombReason reason, float detail = 0.0f, float span = 0.0f);
    void ClassifyBombMessage(const BombObservation& obs);
    void UpdateMarkerVisibility(const BombObservation& obs, float now);
    void ResetBombTrack();

    void SetHostageIntent(HostageIntent intent, HostageReason reason, int detail = 0);
    void ClassifyHostageMessage(const HostageObservation& obs, HostageTrack& track);

    static const char* GetBombIntentName(BombIntent intent);
    static const char* GetHostageIntentName(HostageIntent intent);
};
