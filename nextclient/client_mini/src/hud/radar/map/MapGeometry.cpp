#include "MapGeometry.h"

#include <cassert>

#include <goldsrc/format/bsp.h>
#include <ncl_math/clip.h>

namespace
{
    void AppendVertex(std::vector<RadarMapVertex>& polygon, const RadarMapVertex& vertex)
    {
        if (polygon.empty() || polygon.back().pos != vertex.pos)
        {
            polygon.push_back(vertex);
        }
    }

    void ClipPlane(std::span<const RadarMapVertex> polygon, int axis, float limit, float direction, std::vector<RadarMapVertex>& clipped)
    {
        clipped.clear();

        if (polygon.empty())
        {
            return;
        }

        clipped.reserve(polygon.size() + 1);

        ncl_math::ClipPolygonHalfSpace<RadarMapVertex>(
            polygon,
            [=](const RadarMapVertex& vertex) { return (vertex.pos[axis] - limit) * direction; },
            [=](const RadarMapVertex& previous, const RadarMapVertex& current, float crossing) {
                RadarMapVertex vertex{
                    ncl_math::Lerp(previous.pos, current.pos, crossing),
                    ncl_math::Lerp(previous.uv, current.uv, crossing),
                    ncl_math::Lerp(previous.lightmap_uv, current.lightmap_uv, crossing),
                };
                vertex.pos[axis] = limit;
                return vertex;
            },
            [&](const RadarMapVertex& vertex) { AppendVertex(clipped, vertex); }
        );

        if (clipped.size() > 1 && clipped.front().pos == clipped.back().pos)
        {
            clipped.pop_back();
        }
    }

    int GetContents(const RadarCollisionHull& hull, ncl_math::Vector3 origin)
    {
        int node_index = hull.root;

        while (node_index >= 0)
        {
            assert((size_t)node_index < hull.nodes.size());

            const RadarCollisionNode& node = hull.nodes[node_index];

            assert(node.plane >= 0 && (size_t)node.plane < hull.planes.size());

            const RadarCollisionPlane& plane = hull.planes[node.plane];
            float distance = plane.normal.Dot(origin) - plane.dist;
            node_index = node.children[distance < 0.0f ? 1 : 0];
        }

        return node_index;
    }
} // namespace

std::vector<RadarMapVertex> MapGeometry_Clip(std::span<const RadarMapVertex> polygon, const ncl_math::Aabb3& bounds)
{
    std::vector<RadarMapVertex> clipped;
    std::vector<RadarMapVertex> scratch;

    MapGeometry_Clip(polygon, bounds, clipped, scratch);

    return clipped;
}

void MapGeometry_Clip(
    std::span<const RadarMapVertex> polygon,
    const ncl_math::Aabb3& bounds,
    std::vector<RadarMapVertex>& clipped,
    std::vector<RadarMapVertex>& scratch
)
{
    assert(&clipped != &scratch);

    std::span<const RadarMapVertex> input = polygon;

    for (int axis = 0; axis < 3; axis++)
    {
        ClipPlane(input, axis, bounds.min[axis], 1.0f, scratch);
        ClipPlane(scratch, axis, bounds.max[axis], -1.0f, clipped);
        input = clipped;
    }

    if (clipped.size() < 3)
    {
        clipped.clear();
    }
}

bool MapGeometry_CheckClearance(const RadarMapGeometry& geometry, ncl_math::Vector3 feet)
{
    ncl_math::Vector3 origin = feet;
    origin.z -= geometry.crouch_bounds.min.z;

    for (const RadarCollisionHull& hull : geometry.collision_hulls)
    {
        int contents = GetContents(hull, origin - hull.offset);

        if (contents == goldsrc::CONTENTS_SOLID || contents == goldsrc::CONTENTS_SKY)
        {
            return false;
        }
    }

    return true;
}
