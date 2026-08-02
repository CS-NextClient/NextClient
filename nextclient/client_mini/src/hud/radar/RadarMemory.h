#pragma once

#include <array>

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

// World-space movement trails for the advanced radar: one ring buffer of recent
// positions per player. Pure state: sampling, jump breaks and the post-reset
// suppression window live here; projection and drawing stay with the caller.
class PlayerTracks
{
public:
    static constexpr int kMaxPoints = 34; // must precede Buf below

private:
    struct Buf
    {
        std::array<ncl_math::Vector2, kMaxPoints> points;
        int count;
        int head;
        float last_time;
    };

    static constexpr int kMaxPlayers = 64; // must precede the array below

    std::array<Buf, kMaxPlayers + 1> tracks_ = {};
    bool reset_pending_ = false;
    float suppress_until_ = 0.0f;

    static constexpr float kInterval = 0.10f;
    static constexpr float kJumpDist = 300.0f; // trail break threshold between two samples
    static constexpr float kResetDelay = 0.5f;

public:
    // Wipes every trail and arms the suppression window (resolved by the next
    // BeginFrame), so trails restart at the spawns after a round reset instead of
    // tracing the engine's reset teleport interpolation.
    void Reset();
    void BeginFrame(float time);
    bool Suppressed(float time) const;

    void Clear(int player);

    // Throttled append of the player's current position; a jump beyond the break
    // distance drops the trail (respawn, observer switch, teleport).
    void Sample(int player, ncl_math::Vector2 pos, float time);

    // Copies the trail oldest-to-newest into out (kMaxPoints capacity).
    int CopyPoints(int player, ncl_math::Vector2* out) const;
};

// Fading X marks where players died, kept in a fixed ring so the oldest mark is
// recycled. Pure state: expiry and the hold-then-fade alpha curve live here;
// projection and drawing stay with the caller.
class DeathMarks
{
public:
    struct Visible
    {
        ncl_math::Vector2 pos; // world xy
        ncl_math::Color color;
        int alpha; // 0..255: opaque through the hold window, then fading out
    };

    static constexpr int kMax = 32; // must precede the array below

private:
    struct Mark
    {
        ncl_math::Vector2 pos;
        ncl_math::Color color;
        float time;
        bool active;
    };

    std::array<Mark, kMax> marks_ = {};
    int head_ = 0;

    static constexpr float kHold = 3.0f; // fully opaque before fading
    static constexpr float kLifetime = 4.5f; // hold + 1.5s fade-out

public:
    void Reset();
    void Add(ncl_math::Vector2 world, ncl_math::Color color, float time);

    // Expires stale marks and collects the still-visible ones (out: kMax capacity).
    int CollectVisible(Visible* out, int max, float time);
};

// Last-seen memory for the objective markers: where the bomb was spotted (CT side)
// and where each hostage was last confirmed (T line of sight / CT PVS), so markers
// fade over a short window after sight is lost instead of vanishing. The visibility
// policies deciding WHEN something counts as seen stay with the caller; the slots,
// the fade windows and the position smoothing live here.
class ObjectiveMemory
{
public:
    struct BombSpot
    {
        bool valid;
        ncl_math::Vector2 pos;
        bool planted;
        float last_seen;
    };

    struct HostageSpot
    {
        bool valid;
        int ent;
        ncl_math::Vector2 pos;
        float last_seen;
        bool visible; // seen at the last LOS test; gates per-frame position tracking
    };

    static constexpr int kHostageSlots = 16; // must precede the array below
    static constexpr float kMemory = 1.5f; // seconds of fade memory after sight is lost
    static constexpr float kHold = 0.4f; // fully opaque part of the window

private:
    BombSpot bomb_ = {};
    std::array<HostageSpot, kHostageSlots> hostage_spots_ = {};

    static constexpr float kSmoothRate = 16.0f; // 1/s; ~60 ms time constant
    static constexpr float kSnapDist = 200.0f;

public:
    void Reset();

    BombSpot& bomb()
    {
        return bomb_;
    }
    std::array<HostageSpot, kHostageSlots>& hostages()
    {
        return hostage_spots_;
    }

    void ClearHostageVisibility();

    // Existing slot for the entity, or a fresh one at pos; -1 when all slots are taken.
    int FindOrCreate(int ent_index, ncl_math::Vector2 pos, float time);

    // Eases the spot toward target so the marker glides instead of stepping at the
    // snapshot rate; a jump beyond the snap distance teleports it instead
    // (re-acquired after a fade, or an actual teleport).
    void EaseTo(HostageSpot& s, ncl_math::Vector2 target, float dt) const;

    // The same smoothing for a marker that keeps its position outside a spot slot
    // (the re-purposed bomb message, which the server refreshes at its own rate).
    static ncl_math::Vector2 Ease(ncl_math::Vector2 pos, ncl_math::Vector2 target, float dt);
};
