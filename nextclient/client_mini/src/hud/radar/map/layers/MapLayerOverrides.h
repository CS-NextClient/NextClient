#pragma once

#include <span>
#include <string>
#include <vector>

#include "hud/radar/map/MapGeometry.h"

struct RadarLayerOverride
{
    std::string name;
    float min_z{};
    float max_z{};
};

struct RadarLayerRegion
{
    std::string layer;
    ncl_math::Aabb3 bounds{};
};

struct RadarLayerOverrides
{
    std::vector<RadarLayerOverride> layers;
    std::vector<RadarLayerRegion> regions;
    std::vector<ncl_math::Aabb3> hides;
};

bool MapLayerOverrides_Parse(std::span<const std::string> tokens, RadarLayerOverrides& out, std::string& error);
bool MapLayerOverrides_Validate(const RadarLayerOverrides& overrides, std::string& error);
