#pragma once

#include "hud/radar/render/RadarDraw.h"
#include "hud/radar/integration/Performance.h"

#include <array>
#include <span>

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>
#include <goldsrc/fwd.h>

#include "hud/radar/RadarFrame.h"
#include "hud/radar/RadarLimits.h"
#include "hud/radar/layers/players/DeathMarks.h"
#include "hud/radar/layers/players/PlayerTracks.h"
#include "hud/radar/layers/players/PlayerLastSeen.h"
#include "hud/radar/layers/players/PlayerMarkerFade.h"
#include "hud/radar/layers/players/PlayerMarkerHistory.h"

class RadarVisibility;
class RadarText;
class RadarPolicy;
class RadarBroadcast;

class PlayersLayer
{
public:
    enum class ResetReason
    {
        Hud,
        InitHud,
        Video,
        PlayerClock,
        RadarClock,
        Team,
        Observer,
        Count,
    };

private:
    RadarDrawContext& draw_;
    radar::integration::Performance& performance_;

    struct PlayerJob
    {
        ncl_math::Vector2 pos;
        ncl_math::Vector2 dir; // zero when the facing is unknown
        ncl_math::Vector2 out_dir; // radial outward heading, set only when offscreen
        ncl_math::Color col;
        int alpha;
        int base_alpha;
        float fade_weight;
        int vertical;
        ncl_math::Vector2 indicator;
        bool cone;
        bool carrier;
        bool health;
        int sb_health;
        bool name;
        bool offscreen;
        int index;
        bool is_self;
    };

    // The position selected by the reveal and source policy.
    struct PlayerSpot
    {
        ncl_math::Vector3 pos;
        float yaw;
        bool has_dir;
        bool snapshot;
        bool from_broadcast;
    };

    struct NameWidth
    {
        char name[64];
        int width;
    };

    RadarVisibility& visibility_;
    const RadarPolicy& policy_;
    RadarText& text_;
    const RadarBroadcast& broadcast_;
    PlayerLastSeen last_seen_;
    std::array<PlayerMarkerFade, kRadarMaxPlayers + 1> marker_fade_{};
    RadarTeam team_ = RadarTeam::Unassigned;
    bool spectating_{};
    float last_time_{};

    PlayerTracks tracks_;

    DeathMarks deaths_;
    std::array<bool, kRadarMaxPlayers + 1> prev_alive_{};
    std::array<bool, kRadarMaxPlayers + 1> prev_snapshot_{};
    std::array<ncl_math::Vector3, kRadarMaxPlayers + 1> prev_pos_{};
    std::array<ncl_math::Vector3, kRadarMaxPlayers + 1> prev_snapshot_pos_{};
    std::array<bool, kRadarMaxPlayers + 1> drawn_{};

    std::array<NameWidth, kRadarMaxPlayers + 1> name_widths_{};
    PlayerMarkerHistory marker_history_;
    std::uint64_t fading_players_{};
    std::uint64_t broadcast_players_{};
    std::uint64_t fading_in_players_{};
    std::uint64_t overlapping_players_{};
    std::uint64_t snapshot_step_players_{};
    int marker_resets_{};
    std::array<int, (size_t)ResetReason::Count> reset_counts_{};
    ResetReason reset_reason_ = ResetReason::Hud;
    float reset_previous_time_{};
    float reset_time_{};

public:
    explicit PlayersLayer(
        RadarDrawContext& draw,
        radar::integration::Performance& performance,
        RadarVisibility& visibility,
        const RadarPolicy& policy,
        const RadarBroadcast& broadcast,
        RadarText& text
    );

    void BeginFrame(float time);
    void Draw(const RadarFrame& frame);

    void OnPlayerRespawn(ResetReason reason = ResetReason::Hud, float previous_time = 0.0f, float time = 0.0f);
    void OnVidInit(); // fonts rebuilt: cached name widths are stale
    void DumpMarkers() const;

private:
    void DrawPlayers(const RadarFrame& frame);
    void DrawObserver(const RadarFrame& frame);
    void DrawObserverStatus(const RadarFrame& frame);
    void DrawDeaths(const RadarFrame& frame);
    void DrawLastSeen(const RadarFrame& frame);

    float ComputeHighlightConeSpread(const RadarFrame& frame);
    bool ResolvePlayerSpot(const RadarFrame& frame, int i, bool is_self, goldsrc::cl_entity_t* ent, PlayerSpot& out);
    // Returns the prefix of jobs it filled.
    std::span<const PlayerJob> CollectPlayerJobs(
        const RadarFrame& frame,
        int dot,
        std::span<PlayerJob> jobs,
        bool record_diagnostics
    );

    // Catches a round restart onto a nearby spawn, where the move is too small for the
    // trail's own break distance.
    bool IsTeleported(int player, ncl_math::Vector3 pos, float run_speed, float dt) const;
    // Trails draw as their players are resolved, so they land under every marker layer.
    void UpdatePlayerTrail(
        const RadarFrame& frame,
        int player,
        const goldsrc::cl_entity_t* ent,
        ncl_math::Vector3 world,
        ncl_math::Color color,
        bool offscreen,
        bool teleported,
        bool break_on_jump
    );
    void FillPlayerJob(
        const RadarFrame& frame,
        int player,
        bool is_self,
        RadarTeam team,
        const PlayerSpot& spot,
        const goldsrc::cstrike::extra_player_info_t& ext,
        int dot,
        PlayerJob& job
    ) const;
    void DrawPlayerLayers(const RadarFrame& frame, std::span<const PlayerJob> jobs, int dot, float cone_spread);
    void DrawNames(const RadarFrame& frame, std::span<const PlayerJob> jobs, int dot);
    int ComputeNameHeightPx(const RadarFrame& frame);
    void DrawMarker(ncl_math::Vector2 pos, ncl_math::Vector2 dir, int size, ncl_math::Color color);
    void DrawEdgeArrow(ncl_math::Vector2 pos, ncl_math::Vector2 out_dir, int size, ncl_math::Color color);
    void DrawTrack(const RadarFrame& frame, int player, ncl_math::Color color);
    void RecordPlayerDeathIfNew(int player, RadarTeam team, float time);
    void DrawHealthBar(ncl_math::Vector2 pos, int dot, int sb_health, float opacity);

    // Re-measured only when the name changes: measuring is a full glyph pass.
    int ComputePlayerNameWidth(int player, const char* name);
};
