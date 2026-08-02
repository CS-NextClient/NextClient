#pragma once

#include <array>

#include <ncl_math/vec3.h>

#include "RadarLimits.h"
#include "RadarTeam.h"

class RadarPolicy;
class SmokeRegistry;
struct extra_player_info_t;

// Only enemies already in the local player's PVS can be spotted: an enemy that only a
// remote teammate can see has no position on the client.
class RadarVisibility
{
public:
    explicit RadarVisibility(const SmokeRegistry& smoke) : smoke_(smoke) {}

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

    std::array<Spotted, kRadarMaxPlayers + 1> spotted_{};
    std::array<Viewer, kRadarMaxPlayers + 1> viewers_{};
    int viewer_count_{};
    bool gating_{};
    float last_update_{};
    bool objective_tests_open_{};
    bool traces_begun_{};
    float traces_ms_{};

    // Owned by HudRadarAdvanced; outlives this object.
    const SmokeRegistry& smoke_;

public:
    void Update(int self_player_id, RadarTeam my_team, bool spectating, float time, const RadarPolicy& policy);

    // False for spectators.
    bool is_gating() const
    {
        return gating_;
    }

    // out_alpha is 0..1, fading over the memory window; pos and yaw freeze when sight is
    // lost.
    bool ShouldShowEnemy(int player_id, float time, ncl_math::Vector2& out_pos, float& out_yaw, float& out_alpha) const;

    // Reads the viewer set Update builds.
    bool CheckPointVisible(const ncl_math::Vector3& origin, float time);

    // Optional: batches the trace context around CheckPointVisible calls, which otherwise
    // self-manage.
    void BeginObjectiveTests();
    void EndObjectiveTests();

    void Reset();

    float get_traces_ms() const
    {
        return traces_ms_;
    }
    void ResetStats()
    {
        traces_ms_ = 0.0f;
    }

private:
    void BeginTraces();

    // The viewer build and the enemy sweep are separate passes: the trace context must be
    // open for the second and must not be open for the first.
    struct SweepContext
    {
        int self_player_id;
        RadarTeam my_team;
        int max_clients; // GetMaxClients(), never the array size
        int msgnum; // the local player's snapshot number: an entity from an older one is stale
        extra_player_info_t* extra;
    };

    void AddLocalViewer();
    void AddTeamViewers(const SweepContext& ctx, const RadarPolicy& policy);
    void SpotEnemies(const SweepContext& ctx, const RadarPolicy& policy, float time);
    bool AnyViewerSees(const ncl_math::Vector3 (&target_points)[2], float time, float tolerance) const;
};
