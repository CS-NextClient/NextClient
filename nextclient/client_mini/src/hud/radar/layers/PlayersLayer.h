#pragma once

#include <array>

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

#include "../RadarFrame.h"
#include "../RadarMemory.h"

class RadarVisibility;
class RadarPolicy;
typedef struct cl_entity_s cl_entity_t;

// Player pass of the advanced radar: resolves where each player is drawn from
// (live entity / teammate broadcast / line-of-sight memory), draws movement trails,
// then the layered markers (FOV cones, carrier halos, dots, health bars, names),
// and owns the per-round player state (trails, death marks, previous positions).
class PlayersLayer
{
private:
    // One drawable player, resolved by the collect pass and consumed by the layered
    // draw passes (cones under dots under health/names).
    struct PlayerJob
    {
        ncl_math::Vector2 pos;
        ncl_math::Vector2 dir; // zero when the facing is unknown
        ncl_math::Vector2 out_dir; // radial outward heading, set only when offscreen
        ncl_math::Color col;
        int alpha;
        bool cone; // the controlled/observed player gets an FOV cone
        bool carrier; // bomb-carrier halo
        float halo;
        bool health;
        int sb_health;
        bool name;
        bool offscreen; // clamped to the rim: drawn as a direction arrow, no dot/name/health
        int index;
    };
    // Where a player is drawn from this frame: world xy, facing and fade.
    struct PlayerSpot
    {
        ncl_math::Vector2 pos;
        float yaw;
        int alpha;
        bool has_dir;
    };
    struct NameWidth
    {
        char name[64];
        int width;
    };

    static constexpr int kMaxPlayers = 64; // must precede the arrays below

    RadarVisibility& visibility_;
    const RadarPolicy& policy_;

    PlayerTracks tracks_;

    // Death marks appear on the alive->dead transition of each player, placed at
    // the last known radar position.
    DeathMarks deaths_;
    std::array<bool, kMaxPlayers + 1> prev_alive_ = {};
    std::array<ncl_math::Vector2, kMaxPlayers + 1> prev_pos_ = {};

    std::array<NameWidth, kMaxPlayers + 1> name_widths_ = {};

public:
    PlayersLayer(RadarVisibility& visibility, const RadarPolicy& policy);

    void BeginFrame(float time);
    void DrawPlayers(const RadarFrame& frame);
    void DrawDeaths(const RadarFrame& frame);

    void ResetRound();
    void OnVidInit(); // fonts rebuilt: cached name widths are stale

private:
    float HighlightConeSpread(const RadarFrame& frame);
    bool ResolvePlayerSpot(const RadarFrame& frame, int i, bool is_self, cl_entity_t* ent, PlayerSpot* out);
    int CollectPlayerJobs(const RadarFrame& frame, int dot, PlayerJob* jobs);
    void DrawPlayerLayers(const RadarFrame& frame, const PlayerJob* jobs, int njobs, int dot, float cone_spread);
    void DrawNames(const RadarFrame& frame, const PlayerJob* jobs, int njobs, int dot);
    int NameHeightPx(const RadarFrame& frame);
    void DrawMarker(ncl_math::Vector2 pos, ncl_math::Vector2 dir, int size, ncl_math::Color color, float halo_radius = 0.0f);
    // Draws an outward-pointing arrowhead for a player clamped to the radar rim, replacing the dot.
    void DrawEdgeArrow(ncl_math::Vector2 pos, ncl_math::Vector2 out_dir, int size, ncl_math::Color color);
    void DrawTrack(const RadarFrame& frame, int player, ncl_math::Vector2 pos, ncl_math::Color color);
    void RecordPlayerDeathIfNew(int player, int teamnumber, float time);
    void DrawHealthBar(ncl_math::Vector2 pos, int dot, int sb_health);

    // Console-font width of the player's name, re-measured only when the name changes
    // (measuring is a full glyph pass).
    int PlayerNameWidth(int player, const char* name);
};
