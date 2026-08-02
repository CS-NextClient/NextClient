#pragma once

#include <array>

#include <ncl_math/vec3.h>

class RadarPolicy;

// Gates which enemies the advanced radar may reveal. An enemy is shown only
// while some allied player has a clear line of sight to it: not blocked by world
// geometry or by smoke, and the viewer is not blinded by a flashbang. A short
// "last seen" memory keeps a just-lost marker from flickering.
//
// Who counts as an enemy comes from RadarPolicy, not from the team numbers alone: a
// teammate the server stopped revealing (free-for-all) is spotted like an enemy and
// does not lend its eyes to the team.
//
// Only enemies the server already streams to us (the local player's PVS) can be
// considered; positions of enemies that only a remote teammate can see are not
// available on the client, so true map-wide spotting is out of reach.
class RadarVisibility
{
private:
    struct Spotted
    {
        float last_seen;
        ncl_math::Vector2 pos;
        float yaw;
        bool valid;
    };

    struct Viewer
    {
        ncl_math::Vector3 eye;
        ncl_math::Vector3 fwd;
        float cos_half;
    };

    static constexpr int kMaxPlayers = 64; // must precede the arrays below

    std::array<Spotted, kMaxPlayers + 1> spotted_ = {};
    std::array<Viewer, kMaxPlayers + 1> viewers_ = {};
    int nviewers_ = 0;
    bool gating_ = false;
    float last_update_ = 0.0f;
    bool batch_armed_ = false; // between Begin/EndObjectiveTests
    bool trace_open_ = false; // the deferred batch setup has run (see BeginObjectiveTests)
    float traces_ms_ = 0.0f;

public:
    void Update(int self_idx, int my_team, bool spectating, float time, const RadarPolicy& policy);

    // False for spectators: the caller then draws everyone in the PVS as before.
    bool gating() const
    {
        return gating_;
    }

    // True if enemy slot idx may be drawn; fills its last-known world x/y, the
    // last-known facing yaw, and a 0..1 opacity that fades out over the memory
    // window after sight is lost. Position/yaw freeze the moment sight is lost.
    bool ShouldShowEnemy(int idx, float time, ncl_math::Vector2* out_pos, float* out_yaw, float* out_alpha) const;

    // Direct line of sight to a world point right now, reusing the team viewers
    // built by Update. Used to gate objectives (hostages, the dropped bomb) that
    // should only show while actually seen. Must be called after Update.
    bool PointVisible(const ncl_math::Vector3& origin, float time);

    // Set up the player-prediction trace context once around a batch of
    // PointVisible calls so each call does not rebuild the physent list. The setup
    // itself is deferred to the first PointVisible of the batch, so frames whose
    // tests are all throttled away pay nothing. Outside a batch it self-manages.
    void BeginObjectiveTests();
    void EndObjectiveTests();

    void Reset();

    // CPU time of the LOS trace work accumulated since ResetStats (shown by the
    // hud_radar_debug overlay).
    float traces_ms() const
    {
        return traces_ms_;
    }
    void ResetStats()
    {
        traces_ms_ = 0.0f;
    }

private:
    void BeginTraces();
    bool AnyViewerSees(const ncl_math::Vector3 (&pts)[2], float time, float margin) const;
};
