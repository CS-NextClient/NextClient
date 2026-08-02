#pragma once

#include "RadarTeam.h"
#include "RadarView.h"

typedef struct cl_entity_s cl_entity_t;

// highlight_* follows the observed player while spectating; local_* always reflects
// the local entity, which is what the bomb and hostage draws gate on.
struct FrameContext
{
    cl_entity_t* local;
    bool spectating;
    int self_idx;
    int highlight_idx;
    RadarTeam highlight_team;
    RadarTeam local_team;
};

struct RadarFrame
{
    RadarBox box;
    RadarTransform t;
    float time;
    float dt;
    FrameContext ctx;
    int flash_alpha; // 0..255, 0 when not blinding
    RadarShape shape;
};
