#include "RadarPolicy.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace
{
    float Distance2D(const ncl_math::Vector3& a, const ncl_math::Vector3& b)
    {
        return (ncl_math::Vector2{a.x, a.y} - ncl_math::Vector2{b.x, b.y}).Length();
    }
} // namespace

void RadarPolicy::SetLogger(LogFn fn)
{
    log_ = fn;
}

void RadarPolicy::SetLogging(bool enabled)
{
    if (enabled == logging_)
    {
        return;
    }

    logging_ = enabled;

    if (enabled)
    {
        LogSnapshot();
    }
}

void RadarPolicy::OnLevelChange()
{
    allies_.fill(Ally{});
    hostages_.fill(HostageTrack{});
    ResetBombTrack();

    site_count_ = 0;
    bomb_hint_ = MessageHint::None;
    bomb_intent_ = BombIntent::Unknown;
    bomb_reason_ = BombReason::None;
    bomb_seq_ = 0;
    marker_visible_ = false;
    force_gating_server_ = false;

    map_hostage_count_ = 0;
    hostage_hint_ = MessageHint::None;
    hostage_intent_ = HostageIntent::Unknown;
    hostage_reason_ = HostageReason::None;
    hostages_revealed_ = true;
    hostage_reveal_reason_ = HostageRevealReason::None;
}

void RadarPolicy::SetMapObjectives(int bomb_sites, int hostages)
{
    site_count_ = bomb_sites;
    map_hostage_count_ = hostages;
}

void RadarPolicy::SetBombHint(MessageHint hint)
{
    if (hint == bomb_hint_)
    {
        return;
    }

    bomb_hint_ = hint;

    if (hint == MessageHint::Marker)
    {
        SetBombIntent(BombIntent::Marker, BombReason::Hint);
    }
    else if (hint == MessageHint::Stock)
    {
        SetBombIntent(BombIntent::Bomb, BombReason::Hint);
    }
}

void RadarPolicy::SetHostageHint(MessageHint hint)
{
    if (hint == hostage_hint_)
    {
        return;
    }

    hostage_hint_ = hint;

    if (hint == MessageHint::Marker)
    {
        SetHostageIntent(HostageIntent::Marker, HostageReason::Hint);
    }
    else if (hint == MessageHint::Stock)
    {
        SetHostageIntent(HostageIntent::Hostage, HostageReason::Hint);
    }
}

void RadarPolicy::SetServerFfaHint(bool ffa)
{
    force_gating_server_ = ffa;
}

void RadarPolicy::SetForceAllyGating(bool on)
{
    force_gating_cvar_ = on;
}

void RadarPolicy::BeginFrame(float time, float run_speed, bool applicable)
{
    time_ = time;
    applicable_ = applicable;
    stale_dist_ = std::max(kMinStaleDist, run_speed * kStaleSpeedSeconds);
}

void RadarPolicy::ObserveAlly(int index, bool in_pvs, ncl_math::Vector2 live_pos, const ncl_math::Vector3& broadcast)
{
    if (index < 1 || index > kMaxPlayers || !applicable_)
    {
        return;
    }

    Ally& a = allies_[index];

    if (force_gating_server_ || force_gating_cvar_)
    {
        DecideAlly(index, false, force_gating_server_ ? AllyReason::ForcedServer : AllyReason::ForcedCvar);
        return;
    }

    if (broadcast != a.broadcast)
    {
        a.broadcast = broadcast;
        a.drift_since = -1.0f; // a fresh broadcast re-syncs the position
    }

    if (broadcast == ncl_math::Vector3{})
    {
        DecideAlly(index, false, a.ever_broadcast ? AllyReason::Zeroed : AllyReason::NoBroadcast);
        return;
    }

    a.ever_broadcast = true;

    // Out of the PVS there is nothing to compare against, and the broadcast is exactly
    // what the stock radar would draw.
    if (!in_pvs)
    {
        a.drift_since = -1.0f;
        DecideAlly(index, true, AllyReason::Broadcast);
        return;
    }

    float lag = (live_pos - ncl_math::Vector2{broadcast.x, broadcast.y}).Length();

    if (lag <= stale_dist_)
    {
        a.drift_since = -1.0f;
        DecideAlly(index, true, AllyReason::Broadcast);
        return;
    }

    if (a.drift_since < 0.0f)
    {
        a.drift_since = time_;
        a.drift_start_pos = live_pos;
    }

    // Past the grace window, and only once the ally has moved far enough that the server
    // owed us an update, it is no longer tracking this ally for us: either the message is
    // blocked or the mode stopped sending it.
    if (time_ - a.drift_since >= kStaleTime && (live_pos - a.drift_start_pos).Length() >= kStaleMoveDist)
    {
        DecideAlly(index, false, AllyReason::Stale, lag);
        return;
    }

    if (!a.decided)
    {
        DecideAlly(index, true, AllyReason::Broadcast);
    }
}

void RadarPolicy::ObserveBomb(const BombObservation& obs, float now)
{
    if (obs.seq != bomb_seq_)
    {
        bomb_seq_ = obs.seq;

        if (obs.active)
        {
            ClassifyBombMessage(obs);
        }
        else
        {
            ResetBombTrack(); // picked up or hidden: the next drop is a new episode
        }
    }

    UpdateMarkerVisibility(obs, now);
}

void RadarPolicy::ObserveHostageBroadcast(bool ct_side, int active_count)
{
    // Only a Counter-Terrorist on a map that actually holds hostages can conclude
    // anything from an empty broadcast.
    bool applicable = ct_side && map_hostage_count_ > 0;
    bool revealed = !applicable || active_count > 0;
    HostageRevealReason reason =
        !applicable ? HostageRevealReason::None : (revealed ? HostageRevealReason::Broadcast : HostageRevealReason::NoBroadcast);

    if (revealed == hostages_revealed_ && reason == hostage_reveal_reason_)
    {
        return;
    }

    hostages_revealed_ = revealed;
    hostage_reveal_reason_ = reason;

    if (reason == HostageRevealReason::Broadcast)
    {
        Log("hostages shown from their positions: the server broadcasts them");
    }
    else if (reason == HostageRevealReason::NoBroadcast)
    {
        Log("hostages gated by line of sight: the server sends no hostage broadcast");
    }
}

void RadarPolicy::ObserveHostage(const HostageObservation& obs, float now)
{
    if (obs.index < 0 || obs.index >= kMaxHostages)
    {
        return;
    }

    HostageTrack& track = hostages_[obs.index];

    if (obs.seq != track.seq)
    {
        track.seq = obs.seq;
        ClassifyHostageMessage(obs, track);
    }

    bool visible = hostage_intent_ == HostageIntent::Marker && now - obs.message_time <= kMarkerTimeout;

    if (visible == track.marker_visible)
    {
        return;
    }

    bool timed_out = !visible && hostage_intent_ == HostageIntent::Marker;
    track.marker_visible = visible;

    if (timed_out)
    {
        Log("server marker %d hidden: no update for %.1fs", obs.index, kMarkerTimeout);
    }
}

bool RadarPolicy::HostageMarkerVisible(int index) const
{
    if (index < 0 || index >= kMaxHostages)
    {
        return false;
    }

    return hostages_[index].marker_visible;
}

bool RadarPolicy::AllyRevealed(int index) const
{
    if (!applicable_)
    {
        return true;
    }

    if (index < 1 || index > kMaxPlayers)
    {
        return false;
    }

    return allies_[index].revealed;
}

void RadarPolicy::DecideAlly(int index, bool revealed, AllyReason reason, float detail)
{
    Ally& a = allies_[index];

    if (a.decided && a.revealed == revealed && a.reason == reason)
    {
        return;
    }

    a.decided = true;
    a.revealed = revealed;
    a.reason = reason;

    switch (reason)
    {
        case AllyReason::Broadcast:
            Log("ally %d shown from its position: the server broadcasts it", index);
            break;
        case AllyReason::NoBroadcast:
            Log("ally %d gated by line of sight: no radar broadcast received for it", index);
            break;
        case AllyReason::Zeroed:
            Log("ally %d gated by line of sight: the server zeroed its radar broadcast (free-for-all)", index);
            break;
        case AllyReason::Stale:
            Log("ally %d gated by line of sight: broadcast stuck %.0f units behind for %.1fs", index, detail, kStaleTime);
            break;
        case AllyReason::ForcedCvar:
            Log("ally %d gated by line of sight: forced by hud_radar_allies 0", index);
            break;
        case AllyReason::ForcedServer:
            Log("ally %d gated by line of sight: forced by serverinfo nc_ffa", index);
            break;
        case AllyReason::None:
            break;
    }
}

void RadarPolicy::SetBombIntent(BombIntent intent, BombReason reason, float detail, float span)
{
    if (intent == bomb_intent_ && reason == bomb_reason_)
    {
        return;
    }

    bomb_intent_ = intent;
    bomb_reason_ = reason;

    switch (reason)
    {
        case BombReason::Sites:
            Log("bomb message treated as the bomb: the map has %d bomb target(s) and the position holds still", site_count_);
            break;
        case BombReason::NoSites:
            Log("bomb message treated as a server marker: the map has no bomb targets");
            break;
        case BombReason::PlantedMoved:
            Log("bomb message treated as a server marker: a planted bomb moved %.0f units", detail);
            break;
        case BombReason::Moved:
            Log("bomb message treated as a server marker: the position travelled %.0f units over %.1fs", detail, span);
            break;
        case BombReason::Hint:
            Log("bomb message treated as %s: declared by serverinfo nc_bombmsg", BombIntentName(intent));
            break;
        case BombReason::None:
            break;
    }
}

void RadarPolicy::ClassifyBombMessage(const BombObservation& obs)
{
    float step = track_.messages > 0 ? Distance2D(obs.pos, track_.pos) : 0.0f;
    bool was_planted = track_.planted;

    if (track_.messages == 0)
    {
        track_.first_time = obs.message_time;
    }

    track_.messages++;
    track_.moved += step;
    track_.last_time = obs.message_time;
    track_.pos = obs.pos;
    track_.planted = obs.planted;

    // A declared mode wins over the heuristics, and Marker is a latch: once the server
    // has been seen using the message for something else, it keeps that meaning for the
    // rest of the map.
    if (bomb_hint_ != MessageHint::None || bomb_intent_ == BombIntent::Marker)
    {
        return;
    }

    if (site_count_ == 0)
    {
        SetBombIntent(BombIntent::Marker, BombReason::NoSites);
        return;
    }

    if (obs.planted && was_planted && step >= kPlantedMoveDist)
    {
        SetBombIntent(BombIntent::Marker, BombReason::PlantedMoved, step);
        return;
    }

    float span = track_.last_time - track_.first_time;

    if (track_.messages >= kMoveMessages && track_.moved >= kMoveDist && span >= kMoveWindow)
    {
        SetBombIntent(BombIntent::Marker, BombReason::Moved, track_.moved, span);
        return;
    }

    if (bomb_intent_ == BombIntent::Unknown)
    {
        SetBombIntent(BombIntent::Bomb, BombReason::Sites);
    }
}

void RadarPolicy::SetHostageIntent(HostageIntent intent, HostageReason reason, int detail)
{
    if (intent == hostage_intent_ && reason == hostage_reason_)
    {
        return;
    }

    hostage_intent_ = intent;
    hostage_reason_ = reason;

    switch (reason)
    {
        case HostageReason::MapHostages:
            Log("hostage message treated as a hostage: the map holds %d of them", map_hostage_count_);
            break;
        case HostageReason::NoMapHostages:
            Log("hostage message treated as a server marker: the map has no hostages");
            break;
        case HostageReason::IndexOutOfRange:
            Log("hostage message treated as a server marker: index %d past the map's %d hostage(s)", detail, map_hostage_count_);
            break;
        case HostageReason::TooFast:
            Log("hostage message treated as a server marker: %d updates for one index in %.0fs, above the rate CHostage can send",
                detail,
                kHostageRateWindow);
            break;
        case HostageReason::Hint:
            Log("hostage message treated as %s: declared by serverinfo nc_hostagemsg", HostageIntentName(intent));
            break;
        case HostageReason::None:
            break;
    }
}

void RadarPolicy::ClassifyHostageMessage(const HostageObservation& obs, HostageTrack& track)
{
    if (obs.message_time - track.window_start >= kHostageRateWindow)
    {
        track.window_start = obs.message_time;
        track.window_count = 0;
    }

    track.window_count++;

    // A declared mode wins over the heuristics, and Marker is a latch for the rest of
    // the map, as with the bomb message.
    if (hostage_hint_ != MessageHint::None || hostage_intent_ == HostageIntent::Marker)
    {
        return;
    }

    if (map_hostage_count_ == 0)
    {
        SetHostageIntent(HostageIntent::Marker, HostageReason::NoMapHostages);
        return;
    }

    // CHostage hands out indices 1..N in spawn order, so anything outside that range
    // was written by something else.
    if (obs.index < 1 || obs.index > map_hostage_count_)
    {
        SetHostageIntent(HostageIntent::Marker, HostageReason::IndexOutOfRange, obs.index);
        return;
    }

    if (track.window_count > kHostageRateLimit)
    {
        SetHostageIntent(HostageIntent::Marker, HostageReason::TooFast, track.window_count);
        return;
    }

    if (hostage_intent_ == HostageIntent::Unknown)
    {
        SetHostageIntent(HostageIntent::Hostage, HostageReason::MapHostages);
    }
}

void RadarPolicy::UpdateMarkerVisibility(const BombObservation& obs, float now)
{
    bool visible = obs.active && bomb_intent_ == BombIntent::Marker && now - obs.message_time <= kMarkerTimeout;

    if (visible == marker_visible_)
    {
        return;
    }

    bool timed_out = !visible && obs.active && bomb_intent_ == BombIntent::Marker;
    marker_visible_ = visible;

    if (timed_out)
    {
        Log("server marker hidden: no update for %.1fs", kMarkerTimeout);
    }
}

void RadarPolicy::ResetBombTrack()
{
    track_ = BombTrack{};
}

const char* RadarPolicy::BombIntentName(BombIntent intent)
{
    switch (intent)
    {
        case BombIntent::Bomb:
            return "the bomb";
        case BombIntent::Marker:
            return "a server marker";
        case BombIntent::Unknown:
            break;
    }

    return "undecided";
}

const char* RadarPolicy::HostageIntentName(HostageIntent intent)
{
    switch (intent)
    {
        case HostageIntent::Hostage:
            return "a hostage";
        case HostageIntent::Marker:
            return "a server marker";
        case HostageIntent::Unknown:
            break;
    }

    return "undecided";
}

void RadarPolicy::Log(const char* format, ...) const
{
    if (!log_ || !logging_)
    {
        return;
    }

    char message[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    log_(message);
}

void RadarPolicy::LogSnapshot() const
{
    int gated = 0;

    for (const Ally& a : allies_)
    {
        if (a.decided && !a.revealed)
        {
            gated++;
        }
    }

    Log("policy: %d ally(ies) gated by line of sight, bomb message = %s, hostage message = %s%s",
        gated,
        BombIntentName(bomb_intent_),
        HostageIntentName(hostage_intent_),
        hostages_revealed_ ? "" : ", hostages gated by line of sight");
}
