#include "RadarMapLayerFixtures.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

#include <goldsrc/format/bsp.h>

namespace
{
    constexpr std::array<ncl_math::Vector2, 4> kUvs{{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}};

    void AddSurface(RadarMapGeometry& geometry, const std::array<ncl_math::Vector3, 4>& positions)
    {
        RadarMapSurface surface{};
        surface.surface_id = (int)geometry.surfaces.size();
        surface.normal = (positions[1] - positions[0]).Cross(positions[3] - positions[0]).Normalized();

        for (size_t i = 0; i < positions.size(); i++)
        {
            surface.vertices.push_back({positions[i], kUvs[i], kUvs[i]});
            geometry.bounds.Include({positions[i].x, positions[i].y});
        }

        geometry.surfaces.push_back(std::move(surface));
    }
} // namespace

void RadarMapLayerFixture::AddFloor(float z, ncl_math::Aabb2 xy)
{
    AddSurface(geometry, {{{xy.min.x, xy.min.y, z}, {xy.max.x, xy.min.y, z}, {xy.max.x, xy.max.y, z}, {xy.min.x, xy.max.y, z}}});
}

void RadarMapLayerFixture::AddRamp(ncl_math::Vector3 low, ncl_math::Vector3 high, float width)
{
    ncl_math::Vector2 direction{high.x - low.x, high.y - low.y};
    assert(direction.LengthSqr() > 0.0f && width > 0.0f);
    ncl_math::Vector2 across = direction.Normalized().Perp() * (width * 0.5f);
    ncl_math::Vector3 offset{across.x, across.y, 0.0f};
    AddSurface(geometry, {{low - offset, high - offset, high + offset, low + offset}});
}

void RadarMapLayerFixture::AddStairs(float low, float high, int steps, ncl_math::Aabb2 xy)
{
    assert(steps > 0);

    for (int i = 0; i < steps; i++)
    {
        float start = (float)i / steps;
        float end = (float)(i + 1) / steps;
        AddFloor(
            ncl_math::Lerp(low, high, end),
            {{ncl_math::Lerp(xy.min.x, xy.max.x, start), xy.min.y}, {ncl_math::Lerp(xy.min.x, xy.max.x, end), xy.max.y}}
        );
    }
}

void RadarMapLayerFixture::AddSolidBox(ncl_math::Aabb3 box)
{
    ncl_math::Vector3 minimum = box.min - geometry.crouch_bounds.max;
    ncl_math::Vector3 maximum = box.max - geometry.crouch_bounds.min;
    RadarCollisionHull hull{};

    for (int axis = 0; axis < 3; axis++)
    {
        ncl_math::Vector3 normal{};
        normal[axis] = 1.0f;
        int minimum_node = axis * 2;
        int maximum_node = minimum_node + 1;
        int next = axis == 2 ? goldsrc::CONTENTS_SOLID : maximum_node + 1;
        hull.planes.push_back({normal, minimum[axis]});
        hull.planes.push_back({normal, maximum[axis]});
        hull.nodes.push_back({minimum_node, {maximum_node, goldsrc::CONTENTS_EMPTY}});
        hull.nodes.push_back({maximum_node, {goldsrc::CONTENTS_EMPTY, next}});
    }

    geometry.collision_hulls.push_back(std::move(hull));
    geometry.bounds.Include({box.min.x, box.min.y});
    geometry.bounds.Include({box.max.x, box.max.y});
}

void RadarMapLayerFixture::AddSupportHull(size_t surface_index)
{
    const RadarMapSurface& surface = geometry.surfaces[surface_index];
    ncl_math::Vector3 corner{};

    for (int axis = 0; axis < 3; axis++)
    {
        corner[axis] = surface.normal[axis] >= 0.0f ? geometry.crouch_bounds.min[axis] : geometry.crouch_bounds.max[axis];
    }

    float dist = surface.normal.Dot(surface.vertices[0].pos - corner);
    geometry.collision_hulls.push_back({{{surface.normal, dist}}, {{0, {goldsrc::CONTENTS_EMPTY, goldsrc::CONTENTS_SOLID}}}, 0, {}});
}


void RadarMapLayerFixture::AddOverlapRoom(size_t lower_surface, size_t upper_surface)
{
    std::array<RadarMapSurface, 2> supports{{geometry.surfaces[lower_surface], geometry.surfaces[upper_surface]}};
    std::array<ncl_math::Aabb2, 2> bounds{{ncl_math::Aabb2::Empty(), ncl_math::Aabb2::Empty()}};

    for (size_t i = 0; i < supports.size(); i++)
    {
        for (const RadarMapVertex& vertex : supports[i].vertices)
        {
            bounds[i].Include({vertex.pos.x, vertex.pos.y});
        }
    }

    float minimum_x = std::min(bounds[0].min.x, bounds[1].min.x);
    float maximum_x = std::max({bounds[0].max.x, bounds[1].max.x, minimum_x + 512.0f});
    float y = std::max(bounds[0].max.y, bounds[1].max.y);

    for (size_t i = 0; i < supports.size(); i++)
    {
        const RadarMapSurface& support = supports[i];
        auto add = [&](ncl_math::Aabb2 xy) {
            std::array<ncl_math::Vector3, 4> positions{
                {{xy.min.x, xy.min.y, 0.0f}, {xy.max.x, xy.min.y, 0.0f}, {xy.max.x, xy.max.y, 0.0f}, {xy.min.x, xy.max.y, 0.0f}}
            };

            const ncl_math::Vector3& origin = support.vertices[0].pos;

            for (ncl_math::Vector3& position : positions)
            {
                position.z =
                    origin.z - (support.normal.x * (position.x - origin.x) + support.normal.y * (position.y - origin.y)) / support.normal.z;
            }

            AddSurface(geometry, positions);
        };

        if (bounds[i].max.y < y)
        {
            add({{bounds[i].min.x, bounds[i].max.y}, {bounds[i].max.x, y}});
        }

        add({{minimum_x, y}, {maximum_x, y + 256.0f}});
    }
}
