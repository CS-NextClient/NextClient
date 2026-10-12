#pragma once

#include "hud/radar/map/MapGeometry.h"

class RadarMapLayerFixture
{
public:
    RadarMapGeometry geometry{ncl_math::Aabb2::Empty(), {{-16.0f, -16.0f, -18.0f}, {16.0f, 16.0f, 18.0f}}};

    void AddFloor(float z, ncl_math::Aabb2 xy);
    void AddRamp(ncl_math::Vector3 low, ncl_math::Vector3 high, float width);
    void AddStairs(float low, float high, int steps, ncl_math::Aabb2 xy);
    void AddSupportHull(size_t surface_index);
    void AddOverlapRoom(size_t lower_surface, size_t upper_surface);
    void AddSolidBox(ncl_math::Aabb3 box);
};
