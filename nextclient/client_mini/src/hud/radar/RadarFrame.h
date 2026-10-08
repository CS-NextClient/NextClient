#pragma once

#include <goldsrc/fwd.h>

#include "RadarTeam.h"
#include "RadarView.h"

// highlight_team follows the observed player; highlight_idx is zero outside a player view.
// local_* always reflects the local entity, which gates bomb and hostage visibility.
struct FrameContext
{
    goldsrc::cl_entity_t* local;
    bool spectating;
    int self_idx;
    int highlight_idx;
    RadarTeam highlight_team;
    RadarTeam local_team;
    bool free_camera;
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
