#pragma once

#include "hud/radar/integration/Performance.h"

#include <array>

#include <ncl_math/vec3.h>
#include <goldsrc/fwd.h>

#include "hud/radar/RadarLimits.h"
#include "hud/radar/RadarTeam.h"

class RadarPolicy;
class SmokeRegistry;

// Only enemies already in the local player's PVS can be spotted: an enemy that only a
// remote teammate can see has no position on the client.
class RadarVisibility
{
private:
    struct Spotted
    {
        ncl_math::Vector3 pos;
        float yaw;
        bool visible;
    };

    struct Viewer
    {
        ncl_math::Vector3 eye;
        ncl_math::Vector3 fwd;
        float cos_half;
    };

    // The viewer build and the enemy sweep are separate passes: the trace context must be
    // open for the second and must not be open for the first.
    struct SweepContext
    {
        int self_player_id;
        RadarTeam my_team;
        int max_clients; // GetMaxClients(), never the array size
        int msgnum; // the current engine snapshot number
        goldsrc::cstrike::extra_player_info_t* extra;
    };

    std::array<Spotted, kRadarMaxPlayers + 1> spotted_{};
    std::array<Viewer, kRadarMaxPlayers + 1> viewers_{};
    int viewer_count_{};
    bool gating_{};
    float last_update_{};
    bool objective_tests_open_{};
    bool traces_begun_{};

    // Owned by HudRadarAdvanced; outlives this object.
    const SmokeRegistry& smoke_;
    radar::integration::Performance& performance_;

public:
    RadarVisibility(const SmokeRegistry& smoke, radar::integration::Performance& performance) :
        smoke_(smoke),
        performance_(performance)
    {}
    void Update(int self_player_id, RadarTeam my_team, bool spectating, float time, const RadarPolicy& policy);

    // False for spectators.
    bool is_gating() const
    {
        return gating_;
    }

    bool IsEnemyVisible(int player_id, ncl_math::Vector3& out_pos, float& out_yaw) const;

    // Reads the viewer set Update builds.
    bool CheckPointVisible(const ncl_math::Vector3& origin, float time);

    // Optional: batches the trace context around CheckPointVisible calls, which otherwise
    // self-manage.
    void BeginObjectiveTests();
    void EndObjectiveTests();

    void Reset();


private:
    void BeginTraces();

    void AddLocalViewer();
    void AddTeamViewers(const SweepContext& ctx, const RadarPolicy& policy);
    void SpotEnemies(const SweepContext& ctx, const RadarPolicy& policy, float time);
    bool AnyViewerSees(const ncl_math::Vector3 (&target_points)[2], float time, float tolerance) const;
};
