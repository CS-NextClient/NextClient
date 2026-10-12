#pragma once

#include <array>

#include <ncl_math/vec3.h>

#include "hud/radar/markers/MarkerFeed.h"
#include "hud/radar/policy/ObjectiveSource.h"
#include "hud/radar/RadarFrame.h"
#include "hud/radar/RadarLimits.h"
#include "ObjectiveEntities.h"

class RadarVisibility;
class RadarPolicy;
class RadarBroadcast;

// The hostages, drawn by whichever pass the policy and the local player's team leave available.
class HostageMarkers
{
    // A remembered hostage position. The key is the edict index in the sight pass and the message
    // index in the server-marker pass, so a change of pass drops the whole set.
    struct Spot
    {
        bool valid;
        int key;
        ncl_math::Vector3 pos;
        float last_seen;
        bool visible;
    };

    RadarVisibility& visibility_;
    const RadarPolicy& policy_;
    const RadarBroadcast& broadcast_;

    MarkerFeed feed_;

    std::array<Spot, kRadarMaxHostages> spots_{};

    // Which draw owns spots_. Revealed means the server hands out every hostage position, so both
    // feeds serve at once and MarkerFeed resolves them instead.
    HostageSource pass_{};

public:
    explicit HostageMarkers(RadarVisibility& visibility, const RadarPolicy& policy, const RadarBroadcast& broadcast);

    void Draw(const RadarFrame& frame, const ObjectiveEntities& entities, int dot);

    void Reset();

    // Drops what only holds while the map does, which is the message index to edict pairing.
    void OnLevelChange();

private:
    void SetPass(HostageSource pass);

    void ClearVisibility();

    // -1 when all slots are taken.
    int FindOrCreate(int key, ncl_math::Vector3 pos, float time);

    void DrawDot(const RadarFrame& frame, ncl_math::Vector3 world, int dot, int alpha);
    void DrawBySight(const RadarFrame& frame, const ObjectiveEntities& entities, int dot);
    void DrawRevealed(const RadarFrame& frame, const ObjectiveEntities& entities, int dot);
    void DrawServerMarkers(const RadarFrame& frame, int size);
};
