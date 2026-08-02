#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <ncl_math/vec3.h>

#include "../MapObjectives.h"
#include "../RadarFrame.h"
#include "../RadarMemory.h"

class RadarVisibility;
class RadarPolicy;
typedef struct cl_entity_s cl_entity_t;
typedef struct model_s model_t;
typedef struct cvar_s cvar_t;

// Objective pass of the advanced radar: one edict sweep per frame feeds the bomb
// and the hostage draws, bomb-site letters come from the BSP entity lump, and a
// last-seen memory fades markers out after line of sight is lost. The visibility
// policies (T line of sight vs CT PVS vs server broadcasts) live here; the traces
// themselves come from RadarVisibility.
class ObjectivesLayer
{
private:
    enum class EntModelKind : uint8_t
    {
        Other,
        Hostage,
        BombDropped,
        BombPlanted,
    };

    struct FrameEntities
    {
        static constexpr int kMaxHostages = 24;
        struct BombEnt
        {
            bool found;
            cl_entity_t* ent;
            bool planted;
            ncl_math::Vector2 pos; // snapshot origin; the consumer applies interpolation
        };
        struct HostageEnt
        {
            int index;
            ncl_math::Vector3 origin;
        };
        BombEnt bomb;
        std::array<HostageEnt, kMaxHostages> hostages;
        int hostage_count;
    };

    RadarVisibility& visibility_;
    const RadarPolicy& policy_;

    RadarMapObjectives objectives_;

    // Model-name classification memoized per model pointer (stable within a level;
    // cleared in OnVidInit), so the per-frame sweep does no string scanning.
    std::unordered_map<const model_t*, EntModelKind> model_kind_;

    FrameEntities frame_ents_ = {};

    // Objective entity indices (hostages + the current bomb world model) discovered by
    // the last full rescan. The scan over the whole edict range is throttled to
    // kObjTestInterval; between scans only these indices are revisited, and each is
    // revalidated per frame (model/messagenum) so a stale index simply drops out.
    std::vector<int> objective_ents_;

    float last_obj_test_ = 0.0f;
    bool obj_test_due_ = false;

    // Last seen positions of the bomb (for CT) and hostages (for T). Its window is
    // distinct from RadarVisibility's enemy spot memory (a different value).
    ObjectiveMemory obj_memory_;

    // Time the bomb became planted, so the radar-ping rings start from the centre
    // at that moment instead of being caught mid-cycle. -1 when not planted.
    float bomb_pulse_start_ = -1.0f;

    // Smoothed screen-space source for a bomb message the server re-purposed as a
    // marker: it refreshes at the server's own rate, and unlike the bomb there is no
    // entity to interpolate against, so the position is eased here instead.
    ncl_math::Vector2 marker_pos_;
    bool marker_valid_ = false;

    // Time the bomb first appeared. The entity position is only interpolated once
    // its history has filled (after ex_interp); until then the static broadcast
    // position is used, so a freshly thrown bomb does not jerk when interpolation
    // catches up.
    float bomb_appear_time_ = -1.0f;

    // Stock-client cvar, resolved once (an engine cvar lookup walks a linked list).
    cvar_t* ex_interp_ = nullptr;

    // The expensive part of the objective markers is the line-of-sight trace, not the
    // position update. The LOS visibility decision is refreshed at this cadence while
    // the marker position tracks its live entity every frame, so the marker stays
    // smooth (the bomb keeps a per-frame LOS test; only the hostages, which can be
    // several at once, are throttled).
    static constexpr float kObjTestInterval = 0.05f;

public:
    ObjectivesLayer(RadarVisibility& visibility, const RadarPolicy& policy);

    // Refreshes the frame entity cache for both the bomb and hostage draws and
    // advances the throttled LOS-test cadence. Call once per frame before the draws.
    void CollectEntities(const RadarFrame& frame);

    // Static objectives read from the BSP, loaded on demand. Their counts tell
    // RadarPolicy what this map can be about: a bomb or hostage message on a map
    // holding neither is being used for something else.
    const RadarMapObjectives& map_objectives();

    void DrawBomb(const RadarFrame& frame);
    void DrawSites(const RadarFrame& frame);
    void DrawHostages(const RadarFrame& frame);

    void ResetRound();
    void OnVidInit();

private:
    EntModelKind ClassifyModel(const model_t* model);
    int SiteHeightPx(const RadarFrame& frame);
    void DrawServerMarker(const RadarFrame& frame, int size);
    void DrawServerHostageMarkers(const RadarFrame& frame, int size);
    // Shared shape for every message the server re-purposed as a marker, so a bomb and
    // a hostage message used that way read the same.
    void DrawMarkerShape(ncl_math::Vector2 screen, int size);
};
