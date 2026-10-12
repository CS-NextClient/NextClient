#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hud/radar/map/MapGeometry.h"

namespace taskcoro
{
    class CancellationToken;
}

struct RadarMapReachability
{
    bool available{};
    size_t regions{};
    size_t connected_regions{};
    size_t valid_entry_points{};
    std::vector<int> excluded_surfaces;
    std::string reason;
};

bool MapReachability_ReadEntryPoints(std::string_view entities, float crouch_min_z, std::vector<ncl_math::Vector3>& out);
RadarMapReachability MapReachability_Analyze(const RadarMapGeometry& geometry, const taskcoro::CancellationToken* token = nullptr);
