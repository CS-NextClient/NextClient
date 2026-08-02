#pragma once

#include "RadarView.h"

typedef struct cl_entity_s cl_entity_t;

// Per-frame "who we are / observe" context, computed once per radar frame and
// reused by every marker draw instead of each re-querying the engine. highlight_*
// follows the observed player while spectating; local_* always reflects the local
// entity (the bomb and hostage draws gate on the local team, not the observed one).
struct FrameContext
{
    cl_entity_t* local;
    bool spectating;
    int self_idx;
    int highlight_idx;
    int highlight_team;
    int local_team;
};

// Everything a radar draw layer needs for one frame, bundled so layer methods take
// one argument instead of re-deriving shared state.
struct RadarFrame
{
    RadarBox box;
    RadarTransform t;
    float time;
    float dt;
    FrameContext ctx;
    int flash_alpha; // current blinding-flash alpha (0 when not blinding)
    bool round; // hud_radar_round: circular radar shape
};
