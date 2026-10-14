#pragma once

#include "hud/radar/RadarFrame.h"

struct PlayerObserverMarker
{
    ncl_math::Vector3 pos{};
    float yaw{};
    int player{};
};

struct PlayerObserverIcon
{
    ncl_math::Vector2 center{};
    float radius{};
    float half_extent{};
    float line_width{};
};

PlayerObserverMarker PlayerObserver_ChooseMarker(
    const FrameContext& context,
    const goldsrc::cl_entity_t* entity,
    bool alive,
    ncl_math::Vector3 camera,
    float camera_yaw
);
PlayerObserverIcon PlayerObserver_BuildStatusIcon(const RadarBox& box, const RadarShape& shape);
