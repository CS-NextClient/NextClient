#pragma once

#include <array>
#include <span>
#include <vector>

#include <ncl_math/aabb.h>

inline constexpr float kMapSupportNormalZ = 0.7f;

struct RadarMapVertex
{
    ncl_math::Vector3 pos{};
    ncl_math::Vector2 uv{};
    ncl_math::Vector2 lightmap_uv{};
};

struct RadarMapSurface
{
    int surface_id{};
    ncl_math::Vector3 normal{};
    bool sky{};
    bool tiled{};
    std::vector<RadarMapVertex> vertices;
};

struct RadarCollisionPlane
{
    ncl_math::Vector3 normal{};
    float dist{};
};

struct RadarCollisionNode
{
    int plane{};
    std::array<int, 2> children{};
};

struct RadarCollisionHull
{
    std::vector<RadarCollisionPlane> planes;
    std::vector<RadarCollisionNode> nodes;
    int root{};
    ncl_math::Vector3 offset{};
};

struct RadarMapGeometry
{
    ncl_math::Aabb2 bounds{};
    ncl_math::Aabb3 crouch_bounds{};
    std::vector<RadarMapSurface> surfaces;
    std::vector<RadarCollisionHull> collision_hulls;
    std::vector<ncl_math::Vector3> entry_points;
};

std::vector<RadarMapVertex> MapGeometry_Clip(std::span<const RadarMapVertex> polygon, const ncl_math::Aabb3& bounds);

// The input may reference clipped, but not scratch; the output buffers must be distinct.
void MapGeometry_Clip(
    std::span<const RadarMapVertex> polygon,
    const ncl_math::Aabb3& bounds,
    std::vector<RadarMapVertex>& clipped,
    std::vector<RadarMapVertex>& scratch
);

bool MapGeometry_CheckClearance(const RadarMapGeometry& geometry, ncl_math::Vector3 feet);
