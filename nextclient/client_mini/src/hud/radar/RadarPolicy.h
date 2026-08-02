#pragma once

#include <array>
#include <cstdint>

#include <ncl_math/vec3.h>

// Decides what the advanced radar is allowed to reveal on servers whose game mode
// re-purposes the stock Counter-Strike messages.
//
// Free-for-all: teammates are enemies. ReGameDLL keeps sending the Radar message for
// them but zeroes the origin, so the stock radar shows nothing; the advanced radar
// must not fill that gap from the PVS either. The rule is that the server hands out
// the right to reveal an ally, so an ally is drawn ungated only while its broadcast
// keeps tracking it; otherwise it goes through the enemy line-of-sight path.
//
// Zombie modes: the BombDrop message marks a surviving player instead of the C4. A
// bomb that keeps moving (or that moves while flagged as planted) is not a bomb, and
// neither is any bomb message on a map without bomb targets. HostagePos is re-purposed
// the same way; movement cannot tell it apart (hostages walk), so it is judged by what
// the map holds, by the index range the server can legitimately use, and by an update
// rate CHostage cannot produce.
//
// Every decision is a pure transition over the observations fed in each frame, so it
// is unit tested without the engine; the engine-side collection lives in
// HudRadarAdvanced. Decisions are logged with their reason once they change, gated on
// hud_radar_debug by the caller.

enum class BombIntent : std::uint8_t
{
    Unknown, // no bomb message seen on this map yet
    Bomb, // the stock C4
    Marker, // re-purposed by the server to mark something else (a surviving player)
};

enum class HostageIntent : std::uint8_t
{
    Unknown,
    Hostage, // the stock rescue objective
    Marker, // re-purposed to mark something else
};

// Explicit game-mode declaration from the server, read out of the serverinfo string.
// It outranks the heuristics below, and is the only source available before the first
// round has produced any observations.
enum class MessageHint : std::uint8_t
{
    None,
    Stock, // the objective the message is named after
    Marker,
};

class RadarPolicy
{
public:
    using LogFn = void (*)(const char* message);

    static constexpr int kMaxPlayers = 64;

    // Grace period before a frozen broadcast counts as withdrawn. ReGameDLL sends an
    // update at most once a second, and only after the ally moved 64 units, so a short
    // disagreement between broadcast and live position is normal.
    static constexpr float kStaleTime = 3.0f;
    // How far the broadcast may lag the live position, expressed in seconds of running
    // so a server with a raised sv_maxspeed does not trip the check.
    static constexpr float kStaleSpeedSeconds = 2.0f;
    static constexpr float kMinStaleDist = 320.0f;
    // The ally must also have moved this far while the broadcast stayed put, which is
    // what obliges the server to send an update (its own threshold is 64 units). Without
    // it, a standing ally carrying a position left over from the previous round would be
    // read as withheld through freezetime.
    static constexpr float kStaleMoveDist = 128.0f;

    // A planted bomb cannot move at all; anything past the coordinate quantisation is
    // a tracked entity.
    static constexpr float kPlantedMoveDist = 64.0f;
    // A dropped bomb only moves while it is in the air, so travel spread over this
    // many messages and seconds is a marker following something.
    static constexpr int kMoveMessages = 3;
    static constexpr float kMoveDist = 200.0f;
    static constexpr float kMoveWindow = 3.0f;

    // A re-purposed marker is refreshed by the server; BombPickup / HostageK may never
    // arrive, so it is hidden once the updates stop instead of hanging at the last
    // position.
    static constexpr float kMarkerTimeout = 8.0f;

    // CHostage sends at most one position per second (m_flNextRadarTime), so a sustained
    // rate above this over the window belongs to something else. The allowance is wide
    // enough that a burst of messages arriving after a lag spike does not trip it.
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
        Broadcast, // the server keeps broadcasting this ally
        NoBroadcast, // nothing received for it yet
        Zeroed, // the broadcast arrived zeroed (the free-for-all hide sentinel)
        Stale, // the broadcast froze while the ally kept moving
        ForcedCvar,
        ForcedServer,
    };

    enum class BombReason : std::uint8_t
    {
        None,
        Sites, // the map has bomb targets and the position behaves like a bomb
        NoSites, // nothing can be planted on this map
        PlantedMoved,
        Moved,
        Hint, // declared by the server
    };

    enum class HostageReason : std::uint8_t
    {
        None,
        MapHostages, // the map has hostages and the message fits them
        NoMapHostages,
        IndexOutOfRange,
        TooFast, // more updates than CHostage can send
        Hint,
    };

    enum class HostageRevealReason : std::uint8_t
    {
        None,
        Broadcast, // HostagePos is arriving, so the server is revealing them
        NoBroadcast,
    };

    struct Ally
    {
        ncl_math::Vector3 broadcast;
        float drift_since = -1.0f; // when the live position started disagreeing; -1 while it agrees
        ncl_math::Vector2 drift_start_pos; // live position at that moment (see kStaleMoveDist)
        bool ever_broadcast = false;
        bool revealed = false;
        bool decided = false;
        AllyReason reason = AllyReason::None;
    };

    struct BombTrack
    {
        ncl_math::Vector3 pos;
        bool planted = false;
        int messages = 0;
        float moved = 0.0f;
        float first_time = 0.0f;
        float last_time = 0.0f;
    };

    struct HostageTrack
    {
        std::uint32_t seq = 0;
        float window_start = 0.0f;
        int window_count = 0;
        bool marker_visible = false;
    };

    std::array<Ally, kMaxPlayers + 1> allies_ = {};
    std::array<HostageTrack, kMaxHostages> hostages_ = {};
    BombTrack track_;

    LogFn log_ = nullptr;
    bool logging_ = false;

    float time_ = 0.0f;
    float stale_dist_ = kMinStaleDist;
    bool applicable_ = false; // false while spectating or teamless: nothing to gate

    bool force_gating_cvar_ = false;
    bool force_gating_server_ = false;

    int site_count_ = 0;
    MessageHint bomb_hint_ = MessageHint::None;
    BombIntent bomb_intent_ = BombIntent::Unknown;
    BombReason bomb_reason_ = BombReason::None;
    std::uint32_t bomb_seq_ = 0;
    bool marker_visible_ = false;

    int map_hostage_count_ = 0;
    MessageHint hostage_hint_ = MessageHint::None;
    HostageIntent hostage_intent_ = HostageIntent::Unknown;
    HostageReason hostage_reason_ = HostageReason::None;
    bool hostages_revealed_ = true;
    HostageRevealReason hostage_reveal_reason_ = HostageRevealReason::None;

public:
    void SetLogger(LogFn fn);
    // hud_radar_debug: a 0 -> 1 edge prints the decisions already in force, so enabling
    // it mid-map still shows what the radar is doing.
    void SetLogging(bool enabled);

    void OnLevelChange();

    void SetMapObjectives(int bomb_sites, int hostages);
    void SetBombHint(MessageHint hint); // serverinfo nc_bombmsg
    void SetHostageHint(MessageHint hint); // serverinfo nc_hostagemsg
    void SetServerFfaHint(bool ffa); // serverinfo nc_ffa; only ever tightens
    void SetForceAllyGating(bool on); // hud_radar_allies 0

    void BeginFrame(float time, float run_speed, bool applicable);

    // One living ally (the local player excluded). live_pos is only meaningful while
    // the ally is in the PVS; broadcast is its g_PlayerExtraInfo origin.
    void ObserveAlly(int index, bool in_pvs, ncl_math::Vector2 live_pos, const ncl_math::Vector3& broadcast);

    void ObserveBomb(const BombObservation& obs, float now);

    // Whether the server is still sending HostagePos. ct_side is false for anyone the
    // engine never sends it to (it goes to living Counter-Terrorists only), which leaves
    // the answer at "revealed" and logs nothing.
    void ObserveHostageBroadcast(bool ct_side, int active_count);

    // One live hostage broadcast entry, fed every frame it exists.
    void ObserveHostage(const HostageObservation& obs, float now);

    // True when the ally may be drawn from its live/broadcast position; false routes it
    // through the enemy line-of-sight path.
    bool AllyRevealed(int index) const;

    BombIntent bomb_intent() const
    {
        return bomb_intent_;
    }

    // Only meaningful for BombIntent::Marker: false once the server stopped refreshing it.
    bool marker_visible() const
    {
        return marker_visible_;
    }

    HostageIntent hostage_intent() const
    {
        return hostage_intent_;
    }

    // False when the server sends us no hostage positions: the radar then falls back to
    // the line-of-sight path instead of showing whatever the PVS happens to carry.
    bool hostages_revealed() const
    {
        return hostages_revealed_;
    }

    bool HostageMarkerVisible(int index) const;

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

    static const char* BombIntentName(BombIntent intent);
    static const char* HostageIntentName(HostageIntent intent);
};
