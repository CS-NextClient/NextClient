#pragma once

#include "hud/radar/MapObjectives.h"
#include "hud/radar/RadarFrame.h"
#include "objectives/BombMarker.h"
#include "objectives/HostageMarkers.h"
#include "objectives/ObjectiveEntities.h"

class RadarVisibility;
class RadarPolicy;
class RadarBroadcast;
class RadarText;

class ObjectivesLayer
{
    RadarText& text_;

    RadarMapObjectives objectives_;

    ObjectiveEntities objective_entities_;
    BombMarker bomb_;
    HostageMarkers hostages_;

public:
    explicit ObjectivesLayer(RadarVisibility& visibility, const RadarPolicy& policy, const RadarBroadcast& broadcast, RadarText& text);

    // Refills the entity snapshot the draws read and advances the throttled sight cadence.
    void CollectEntities(const RadarFrame& frame);

    void Draw(const RadarFrame& frame);

    void OnPlayerRespawn();
    void OnVidInit();

    // Parses the entity lump on the first call of a level.
    const RadarMapObjectives& get_map_objectives();

private:
    void DrawBomb(const RadarFrame& frame);
    void DrawSites(const RadarFrame& frame);
    void DrawHostages(const RadarFrame& frame);

    int ComputeSiteHeightPx(const RadarFrame& frame);
};
