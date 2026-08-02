#pragma once

#include <ncl_math/vec3.h>

#include "hud/radar/RadarFrame.h"
#include "ObjectiveEntities.h"

class RadarVisibility;
class RadarPolicy;
class RadarBroadcast;
typedef struct cvar_s cvar_t;

// The C4, from whichever of the three feeds serves the local player this frame.
class BombMarker
{
    // Where the bomb is drawn from while the sight of it is remembered.
    struct Spot
    {
        bool valid;
        ncl_math::Vector2 pos;
        bool planted;
        float last_seen;
    };

    RadarVisibility& visibility_;
    const RadarPolicy& policy_;
    const RadarBroadcast& broadcast_;

    Spot spot_{};

    // -1 when not planted.
    float pulse_start_ = -1.0f;

    // Eased here: a re-purposed bomb message has no entity to interpolate against.
    ncl_math::Vector2 marker_pos_;
    bool marker_valid_{};

    // Before the entity history fills (ex_interp) interpolating makes a freshly thrown bomb jerk.
    float appear_time_ = -1.0f;

    bool visible_{};

    // Resolved once: an engine cvar lookup walks a linked list.
    cvar_t* ex_interp_{};

public:
    explicit BombMarker(RadarVisibility& visibility, const RadarPolicy& policy, const RadarBroadcast& broadcast);

    void Draw(const RadarFrame& frame, const ObjectiveEntities& entities, int size);

    void Reset();

private:
    float ComputeInterpLag();
    void UpdatePulse(bool planted, float time);
    void DrawMarker(const RadarFrame& frame, ncl_math::Vector2 world, int size, bool planted, float fade);
    void DrawFromBroadcast(const RadarFrame& frame, const ObjectiveEntities& entities, int size, float interp_lag);
    void DrawFromSight(const RadarFrame& frame, const ObjectiveEntities& entities, int size, float interp_lag);
    void DrawServerMarker(const RadarFrame& frame, int size);
};
