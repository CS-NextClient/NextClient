#pragma once

#include <goldsrc/entity.h>

#include "hud/radar/map/layers/RadarMapLayers.h"
#include "hud/radar/RadarTeam.h"
#include "hud/radar/render/RadarView.h"

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
    int message_num{};
    int observed_idx{}; // observer target, independent of in-eye highlighting
};

class RadarDrawContext;

struct RadarFrame
{
    RadarDrawContext& draw;
    RadarBox box;
    RadarTransform t;
    float time;
    float dt;
    FrameContext ctx;
    int flash_alpha; // 0..255, 0 when not blinding
    RadarShape shape;
    RadarLayerView layer_view{};
};

inline bool RadarFrame_IsEntityCurrent(const FrameContext& context, const goldsrc::cl_entity_t* entity)
{
    return entity && entity->curstate.messagenum == context.message_num;
}

inline ncl_math::Vector3 RadarFrame_GetPlayerPosition(const goldsrc::cl_entity_t& entity)
{
    return ncl_math::Vector3::Ref(entity.curstate.origin);
}

float RadarFrame_NormalizeTime(float time, float previous_time);
