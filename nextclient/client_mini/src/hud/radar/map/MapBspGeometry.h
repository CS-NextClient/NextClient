#pragma once

#include <cstdint>
#include <string>

#include <goldsrc/fwd.h>

#include "hud/radar/map/MapGeometry.h"

struct RadarMapPreparationDiagnostic
{
    std::string waiting_reason;
    uint64_t attempts{};
    int mode{};
    float capture_ms{};
};

const goldsrc::hull_t* MapBspGeometry_SelectPlayerHull(
    goldsrc::playermove_t& movement,
    const goldsrc::event_api_s& events,
    goldsrc::model_t& world,
    int player_hull,
    std::string* waiting_reason = nullptr
);

// False leaves out empty so incomplete world data can be retried.
bool MapBspGeometry_Collect(
    const goldsrc::model_t& world,
    const goldsrc::hull_t& hull,
    const ncl_math::Aabb3& crouch_bounds,
    RadarMapGeometry& out,
    std::string* waiting_reason = nullptr
);
