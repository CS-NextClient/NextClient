#include "RadarMapLayers.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

#include <goldsrc/format/bsp.h>
#include <ncl_math/clip.h>
#include <taskcoro/CancellationToken.h>

namespace
{
    constexpr float kCellSize = 32.0f;
    constexpr float kMergedSpan = 64.0f;
    constexpr float kSignificantOverlapArea = 131072.0f;
    constexpr float kSkyRoofGap = 64.0f;
    constexpr float kSkyRoofTolerance = 2.0f;
    constexpr float kGeometryEpsilon = 0.001f;
    constexpr float kMinArea = 0.0001f;
    constexpr float kUnbounded = 1.0e30f;
    constexpr size_t kLookupLeafSize = 8;
    constexpr int kUnavailableOverride = -2;
    constexpr size_t kSupportPairHashSeed = 0x9e3779b9u;
    const RadarLayerLayout kEmptyLayout;

    bool IsExcluded(std::span<const int> excluded, int source)
    {
        return std::binary_search(excluded.begin(), excluded.end(), source);
    }

    RadarMapSurface FragmentSurface(const RadarMapSurface& surface, std::vector<RadarMapVertex> vertices)
    {
        return {surface.surface_id, surface.normal, surface.sky, surface.tiled, std::move(vertices)};
    }

    float NodeDistanceSquared(const RadarSupportNode& node, ncl_math::Vector3 position, int layer)
    {
        if (layer < 0)
        {
            return (position - node.bounds.ClosestPoint(position)).LengthSqr();
        }

        auto bounds = std::lower_bound(node.layer_bounds.begin(), node.layer_bounds.end(), layer, [](const auto& entry, int id) {
            return entry.first < id;
        });

        if (bounds == node.layer_bounds.end() || bounds->first != layer)
        {
            return std::numeric_limits<float>::infinity();
        }

        return (position - bounds->second.ClosestPoint(position)).LengthSqr();
    }

    struct DetailOwner
    {
        int layer{};
        int automatic_layer{};
        ncl_math::Vector3 normal{};
        ncl_math::Vector3 point{};
    };

    struct SupportPairHash
    {
        size_t operator()(const std::pair<size_t, size_t>& value) const noexcept
        {
            size_t first = std::hash<size_t>{}(value.first);
            size_t second = std::hash<size_t>{}(value.second);

            return first ^ (second + kSupportPairHashSeed + (first << 6) + (first >> 2));
        }
    };

    struct PolygonPlane
    {
        ncl_math::Vector3 normal{};
        ncl_math::Vector3 origin{};
        float offset{};
    };

    struct Candidate
    {
        RadarMapSurface surface;
        ncl_math::Aabb2 bounds = ncl_math::Aabb2::Empty();
        ncl_math::Vector3 center{};
        float area{};
        float minimum{};
        float maximum{};
    };

    struct Platform
    {
        std::vector<size_t> candidates;
        std::set<size_t> neighbors;
        std::unordered_set<size_t> conflicts;
        ncl_math::Aabb2 bounds = ncl_math::Aabb2::Empty();
        float area{};
        float height{};
        float minimum = kUnbounded;
        float maximum = -kUnbounded;
        int surface_id = std::numeric_limits<int>::max();
        int group = -1;
    };

    struct PendingTransition
    {
        std::vector<size_t> platforms;
        std::vector<int> endpoints;
        std::map<int, float> heights;
    };

    struct Group
    {
        std::vector<size_t> platforms;
        ncl_math::Aabb2 bounds = ncl_math::Aabb2::Empty();
        float area{};
        float height{};
        float height_area{};
        float minimum = kUnbounded;
        float maximum = -kUnbounded;
        int surface_id = std::numeric_limits<int>::max();
        std::vector<std::string> reasons;
    };

    bool IsCanceled(const taskcoro::CancellationToken* token)
    {
        return token && token->IsCanceled();
    }

    int Cell(float coordinate)
    {
        return (int)std::floor(coordinate / kCellSize);
    }

    ncl_math::Aabb2 Bounds(std::span<const RadarMapVertex> polygon)
    {
        ncl_math::Aabb2 bounds = ncl_math::Aabb2::Empty();

        for (const RadarMapVertex& vertex : polygon)
        {
            bounds.Include({vertex.pos.x, vertex.pos.y});

        }

        return bounds;
    }

    float SignedArea(std::span<const RadarMapVertex> polygon)
    {
        float area = 0.0f;

        for (size_t i = 1; i + 1 < polygon.size(); i++)
        {
            ncl_math::Vector3 a = polygon[i].pos - polygon[0].pos;
            ncl_math::Vector3 b = polygon[i + 1].pos - polygon[0].pos;

            area += a.x * b.y - b.x * a.y;
        }

        return area * 0.5f;
    }

    ncl_math::Vector3 Centroid(std::span<const RadarMapVertex> polygon)
    {
        ncl_math::Vector3 center{};
        float total = 0.0f;

        for (size_t i = 1; i + 1 < polygon.size(); i++)
        {
            ncl_math::Vector3 a = polygon[i].pos - polygon[0].pos;
            ncl_math::Vector3 b = polygon[i + 1].pos - polygon[0].pos;
            float area = std::abs(a.x * b.y - a.y * b.x);

            center = center + (polygon[0].pos + polygon[i].pos + polygon[i + 1].pos) * (area / 3.0f);
            total += area;
        }

        return center / total;
    }

    float Height(ncl_math::Vector3 normal, ncl_math::Vector3 origin, ncl_math::Vector2 point)
    {
        return origin.z - (normal.x * (point.x - origin.x) + normal.y * (point.y - origin.y)) / normal.z;
    }

    float Height(const RadarMapSurface& surface, ncl_math::Vector2 point)
    {
        return Height(surface.normal, surface.vertices[0].pos, point);
    }

    std::vector<RadarMapVertex> ClipPlane(std::span<const RadarMapVertex> polygon, const PolygonPlane& plane, float direction = 1.0f)
    {
        std::vector<RadarMapVertex> clipped;

        if (polygon.empty())
        {
            return clipped;
        }

        ncl_math::ClipPolygonHalfSpace<RadarMapVertex>(
            polygon,
            [&](const RadarMapVertex& vertex) { return (plane.normal.Dot(vertex.pos - plane.origin) - plane.offset) * direction; },
            [](const RadarMapVertex& previous, const RadarMapVertex& current, float fraction) {
                return RadarMapVertex{
                    ncl_math::Lerp(previous.pos, current.pos, fraction),
                    ncl_math::Lerp(previous.uv, current.uv, fraction),
                    ncl_math::Lerp(previous.lightmap_uv, current.lightmap_uv, fraction),
                };
            },
            [&](const RadarMapVertex& vertex) { clipped.push_back(vertex); }
        );

        return clipped;
    }

    std::vector<RadarMapVertex> ClipSide(std::span<const RadarMapVertex> polygon, ncl_math::Vector2 normal, float dist)
    {
        return ClipPlane(polygon, {{normal.x, normal.y, 0.0f}, {}, dist});

    }

    std::vector<RadarMapVertex> IntersectPolygons(std::span<const RadarMapVertex> polygon, std::span<const RadarMapVertex> footprint)
    {
        std::vector<RadarMapVertex> clipped(polygon.begin(), polygon.end());
        float direction = SignedArea(footprint) >= 0.0f ? 1.0f : -1.0f;

        for (size_t i = 0; i < footprint.size() && !clipped.empty(); i++)
        {
            const ncl_math::Vector3& a = footprint[i].pos;
            const ncl_math::Vector3& b = footprint[(i + 1) % footprint.size()].pos;
            ncl_math::Vector2 normal = ncl_math::Vector2{b.x - a.x, b.y - a.y}.Perp() * direction;

            clipped = ClipSide(clipped, normal, normal.Dot({a.x, a.y}));
        }

        return clipped;
    }

    bool Contains(std::span<const RadarMapVertex> polygon, ncl_math::Vector2 point)
    {
        float direction = SignedArea(polygon) >= 0.0f ? 1.0f : -1.0f;

        for (size_t i = 0; i < polygon.size(); i++)
        {
            const ncl_math::Vector3& a = polygon[i].pos;
            const ncl_math::Vector3& b = polygon[(i + 1) % polygon.size()].pos;
            ncl_math::Vector2 normal = ncl_math::Vector2{b.x - a.x, b.y - a.y}.Perp() * direction;

            if (normal.Dot(point - ncl_math::Vector2{a.x, a.y}) < -kGeometryEpsilon)
            {
                return false;
            }
        }

        return true;
    }

    int OverrideLayer(ncl_math::Vector3 point, const RadarLayerOverrides& overrides, std::span<const int> ids)
    {
        for (const RadarLayerRegion& region : overrides.regions)
        {
            if (region.bounds.ContainsHalfOpen(point))
            {
                for (size_t i = 0; i < overrides.layers.size(); i++)
                {
                    if (overrides.layers[i].name == region.layer)
                    {
                        return ids[i] >= 0 ? ids[i] : kUnavailableOverride;
                    }
                }
            }
        }

        for (size_t i = 0; i < overrides.layers.size(); i++)
        {
            if (point.z >= overrides.layers[i].min_z && point.z < overrides.layers[i].max_z)
            {
                return ids[i] >= 0 ? ids[i] : kUnavailableOverride;
            }
        }

        return -1;
    }

    bool MatchesAssignment(
        const RadarLayerSupport& support,
        ncl_math::Vector3 point,
        const RadarLayerOverrides& overrides,
        std::span<const int> ids
    )
    {
        int assigned = OverrideLayer(point, overrides, ids);
        return assigned != kUnavailableOverride && support.layer == (assigned >= 0 ? assigned : support.automatic_layer);
    }

    std::vector<RadarMapSurface> SplitRegion(const RadarMapSurface& surface, const ncl_math::Aabb3& bounds)
    {
        for (int axis = 0; axis < 3; axis++)
        {
            float minimum = kUnbounded;
            float maximum = -kUnbounded;

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                minimum = std::min(minimum, vertex.pos[axis]);
                maximum = std::max(maximum, vertex.pos[axis]);
            }

            if (maximum < bounds.min[axis] || minimum >= bounds.max[axis])
            {
                return {surface};
            }
        }

        RadarMapSurface inside = surface;
        std::vector<RadarMapSurface> pieces;

        for (int axis = 0; axis < 3; axis++)
        {
            for (int side = 0; side < 2; side++)
            {
                ncl_math::Vector3 normal{};
                normal[axis] = side == 0 ? 1.0f : -1.0f;
                float offset = side == 0 ? bounds.min[axis] : -bounds.max[axis];
                PolygonPlane plane{normal, {}, offset};
                float minimum = kUnbounded;
                float maximum = -kUnbounded;

                for (const RadarMapVertex& vertex : inside.vertices)
                {
                    float distance = plane.normal.Dot(vertex.pos) - plane.offset;
                    minimum = std::min(minimum, distance);
                    maximum = std::max(maximum, distance);
                }

                if (minimum >= 0.0f)
                {
                    continue;
                }

                if (maximum <= 0.0f)
                {
                    pieces.push_back(std::move(inside));
                    return pieces;
                }

                RadarMapSurface outside = FragmentSurface(surface, ClipPlane(inside.vertices, plane, -1.0f));

                if (std::abs(SignedArea(outside.vertices)) > 0.0f)
                {
                    pieces.push_back(std::move(outside));
                }

                inside.vertices = ClipPlane(inside.vertices, plane);
            }
        }

        if (std::abs(SignedArea(inside.vertices)) > 0.0f)
        {
            pieces.push_back(std::move(inside));
        }

        return pieces;
    }

    std::vector<RadarMapSurface> SplitOverrides(const RadarMapSurface& surface, const RadarLayerOverrides& overrides)
    {
        std::vector<RadarMapSurface> pieces{surface};

        for (const RadarLayerOverride& layer : overrides.layers)
        {
            for (float boundary : {layer.min_z, layer.max_z})
            {
                std::vector<RadarMapSurface> clipped;

                for (RadarMapSurface& piece : pieces)
                {
                    float minimum = kUnbounded;
                    float maximum = -kUnbounded;

                    for (const RadarMapVertex& vertex : piece.vertices)
                    {
                        minimum = std::min(minimum, vertex.pos.z);
                        maximum = std::max(maximum, vertex.pos.z);
                    }

                    if (minimum < boundary && maximum > boundary)
                    {
                        for (float direction : {-1.0f, 1.0f})
                        {
                            RadarMapSurface fragment =
                                FragmentSurface(piece, ClipPlane(piece.vertices, {{0.0f, 0.0f, 1.0f}, {}, boundary}, direction));

                            if (std::abs(SignedArea(fragment.vertices)) > 0.0f)
                            {
                                clipped.push_back(std::move(fragment));
                            }
                        }
                    }
                    else
                    {
                        clipped.push_back(std::move(piece));
                    }
                }

                pieces = std::move(clipped);
            }
        }

        for (const RadarLayerRegion& region : overrides.regions)
        {
            std::vector<RadarMapSurface> clipped;

            for (const RadarMapSurface& piece : pieces)
            {
                std::vector<RadarMapSurface> fragments = SplitRegion(piece, region.bounds);

                for (RadarMapSurface& fragment : fragments)
                {
                    clipped.push_back(std::move(fragment));
                }
            }

            pieces = std::move(clipped);
        }

        return pieces;
    }

    bool Coplanar(const RadarMapSurface& a, const RadarMapSurface& b)
    {
        return (a.normal - b.normal).LengthSqr() <= kGeometryEpsilon * kGeometryEpsilon &&
               std::abs(a.normal.Dot(b.vertices[0].pos - a.vertices[0].pos)) <= kGeometryEpsilon;
    }

    ncl_math::Vector3 ClearanceProbe(const RadarMapGeometry& geometry, const Candidate& candidate)
    {
        const ncl_math::Vector3& normal = candidate.surface.normal;
        float x = normal.x > 0.0f ? geometry.crouch_bounds.min.x : geometry.crouch_bounds.max.x;
        float y = normal.y > 0.0f ? geometry.crouch_bounds.min.y : geometry.crouch_bounds.max.y;

        // The flat hull bottom clears the highest point under its XY footprint.
        ncl_math::Vector3 probe = candidate.center;
        probe.z += -(normal.x * x + normal.y * y) / normal.z + kGeometryEpsilon;
        return probe;
    }

    Candidate MakeCandidate(RadarMapSurface surface)
    {
        Candidate candidate{};

        candidate.bounds = Bounds(surface.vertices);
        candidate.area = std::abs(SignedArea(surface.vertices));
        candidate.center = Centroid(surface.vertices);
        candidate.minimum = kUnbounded;
        candidate.maximum = -kUnbounded;

        for (const RadarMapVertex& vertex : surface.vertices)
        {
            candidate.minimum = std::min(candidate.minimum, vertex.pos.z);
            candidate.maximum = std::max(candidate.maximum, vertex.pos.z);
        }

        candidate.surface = std::move(surface);

        return candidate;
    }

    std::vector<std::vector<RadarMapVertex>> ClearIntersection(
        const RadarMapGeometry& geometry,
        const RadarMapSurface& support,
        std::vector<std::vector<RadarMapVertex>> polygons,
        const taskcoro::CancellationToken* token,
        bool raised_contact = false
    );

    struct RenderSkyFace
    {
        ncl_math::Aabb2 bounds{};
        float height{};
        std::span<const RadarMapVertex> footprint;
    };

    std::vector<RenderSkyFace> RenderSkies(const RadarMapGeometry& geometry);
    bool IsRenderRoof(const RadarMapSurface& surface, std::span<const RenderSkyFace> skies);

    std::vector<Candidate> Collect(
        const RadarMapGeometry& geometry,
        std::span<const int> excluded,
        const taskcoro::CancellationToken* token
    )
    {
        std::vector<Candidate> candidates;
        std::vector<RenderSkyFace> skies = RenderSkies(geometry);

        for (const RadarMapSurface& surface : geometry.surfaces)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            if (surface.sky || surface.normal.z < kMapSupportNormalZ || surface.vertices.size() < 3 ||
                IsExcluded(excluded, surface.surface_id) || IsRenderRoof(surface, skies))
            {
                continue;
            }

            ncl_math::Aabb2 bounds = Bounds(surface.vertices);

            for (int x = Cell(bounds.min.x); x <= Cell(bounds.max.x); x++)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                for (int y = Cell(bounds.min.y); y <= Cell(bounds.max.y); y++)
                {
                    RadarMapSurface fragment = FragmentSurface(
                        surface,
                        MapGeometry_Clip(
                            surface.vertices,
                            {{x * kCellSize, y * kCellSize, -kUnbounded}, {(x + 1) * kCellSize, (y + 1) * kCellSize, kUnbounded}}
                        )
                    );

                    if (std::abs(SignedArea(fragment.vertices)) <= kMinArea)
                    {
                        continue;
                    }

                    Candidate candidate = MakeCandidate(std::move(fragment));
                    bool clear = MapGeometry_CheckClearance(geometry, ClearanceProbe(geometry, candidate));

                    if (!clear)
                    {
                        // A tread center can be inside the next riser expanded by the player hull.
                        clear = !ClearIntersection(geometry, candidate.surface, {candidate.surface.vertices}, token).empty();
                    }

                    if (clear)
                    {
                        candidates.push_back(std::move(candidate));
                    }
                }
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return std::tuple{a.center.z, -a.area, a.bounds.min.x, a.bounds.min.y, a.bounds.max.x, a.bounds.max.y, a.surface.surface_id} <
                   std::tuple{b.center.z, -b.area, b.bounds.min.x, b.bounds.min.y, b.bounds.max.x, b.bounds.max.y, b.surface.surface_id};
        });

        return candidates;
    }

    size_t Root(std::vector<size_t>& roots, size_t index)
    {
        while (roots[index] != index)
        {
            roots[index] = roots[roots[index]];
            index = roots[index];
        }

        return index;
    }

    struct Overlap
    {
        size_t lower{};
        size_t upper{};
        std::vector<RadarMapVertex> polygon;
        ncl_math::Aabb2 bounds = ncl_math::Aabb2::Empty();
    };

    std::vector<std::vector<RadarMapVertex>> ClearIntersection(
        const RadarMapGeometry& geometry,
        const RadarMapSurface& support,
        std::vector<std::vector<RadarMapVertex>> polygons,
        const taskcoro::CancellationToken* token,
        bool raised_contact
    )
    {
        Candidate probe{};
        probe.surface = support;
        float offset = ClearanceProbe(geometry, probe).z - geometry.crouch_bounds.min.z;
        float origin_height = Height(support, {0.0f, 0.0f});
        float contact_rise = raised_contact ? geometry.crouch_bounds.max.z : 0.0f;
        std::vector<std::vector<RadarMapVertex>> original;

        if (!raised_contact)
        {
            original = polygons;
        }

        for (const RadarCollisionHull& hull : geometry.collision_hulls)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            std::vector<std::pair<int, std::vector<RadarMapVertex>>> pending;

            for (std::vector<RadarMapVertex>& polygon : polygons)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                pending.emplace_back(hull.root, std::move(polygon));
            }

            polygons.clear();

            while (!pending.empty())
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                int index = pending.back().first;
                std::vector<RadarMapVertex> polygon = std::move(pending.back().second);

                pending.pop_back();

                if (index < 0)
                {
                    if (index != goldsrc::CONTENTS_SOLID && index != goldsrc::CONTENTS_SKY)
                    {
                        polygons.push_back(std::move(polygon));
                    }

                    continue;
                }

                const RadarCollisionNode& node = hull.nodes[index];
                const RadarCollisionPlane& collision = hull.planes[node.plane];
                PolygonPlane plane{
                    {collision.normal.x - collision.normal.z * support.normal.x / support.normal.z,
                     collision.normal.y - collision.normal.z * support.normal.y / support.normal.z,
                     0.0f},
                    {},
                    collision.dist + collision.normal.Dot(hull.offset) - collision.normal.z * (origin_height + offset),
                };
                float minimum = kUnbounded;
                float maximum = -kUnbounded;

                for (const RadarMapVertex& vertex : polygon)
                {
                    float distance = plane.normal.Dot(vertex.pos) - plane.offset;
                    minimum = std::min(minimum, distance + std::min(0.0f, collision.normal.z * contact_rise));
                    maximum = std::max(maximum, distance + std::max(0.0f, collision.normal.z * contact_rise));
                }

                if (minimum >= 0.0f || maximum <= 0.0f)
                {
                    pending.emplace_back(node.children[minimum >= 0.0f ? 0 : 1], std::move(polygon));
                    continue;
                }

                for (int side = 0; side < 2; side++)
                {
                    float direction = side == 0 ? 1.0f : -1.0f;
                    PolygonPlane boundary = plane;
                    boundary.offset -= direction * std::max(0.0f, direction * collision.normal.z) * contact_rise;
                    std::vector<RadarMapVertex> fragment = ClipPlane(polygon, boundary, direction);

                    if (std::abs(SignedArea(fragment)) > kMinArea)
                    {
                        pending.emplace_back(node.children[side], std::move(fragment));
                    }
                }
            }

            if (polygons.empty())
            {
                break;
            }
        }

        if (polygons.empty() && !raised_contact && !IsCanceled(token))
        {
            return ClearIntersection(geometry, support, std::move(original), token, true);
        }

        return polygons;
    }

    std::vector<std::vector<RadarMapVertex>> SubtractFootprint(
        std::span<const RadarMapVertex> polygon,
        std::span<const RadarMapVertex> footprint
    )
    {
        std::vector<std::vector<RadarMapVertex>> outside;
        std::vector<RadarMapVertex> inside(polygon.begin(), polygon.end());
        float direction = SignedArea(footprint) >= 0.0f ? 1.0f : -1.0f;

        for (size_t i = 0; i < footprint.size() && !inside.empty(); i++)
        {
            const ncl_math::Vector3& a = footprint[i].pos;
            const ncl_math::Vector3& b = footprint[(i + 1) % footprint.size()].pos;
            ncl_math::Vector2 normal = ncl_math::Vector2{b.x - a.x, b.y - a.y}.Perp() * direction;

            if (normal.LengthSqr() == 0.0f)
            {
                continue;
            }

            PolygonPlane plane{{normal.x, normal.y, 0.0f}, a, 0.0f};
            std::vector<RadarMapVertex> piece = ClipPlane(inside, plane, -1.0f);

            if (std::abs(SignedArea(piece)) > kMinArea)
            {
                outside.push_back(std::move(piece));
            }

            inside = ClipPlane(inside, plane);
        }

        return outside;
    }

    void SignificantConflicts(
        std::vector<Platform>& platforms,
        std::vector<Overlap>& overlaps,
        RadarLayerAnalysis& analysis,
        const taskcoro::CancellationToken* token
    )
    {
        std::vector<size_t> roots(overlaps.size());
        std::iota(roots.begin(), roots.end(), 0u);
        std::map<std::pair<int, int>, std::vector<size_t>> cells;

        for (size_t i = 0; i < overlaps.size(); i++)
        {
            if (IsCanceled(token))
            {
                return;
            }

            const Overlap& overlap = overlaps[i];
            ncl_math::Vector3 center = Centroid(overlap.polygon);
            std::pair<int, int> cell{Cell(center.x), Cell(center.y)};

            for (int x = cell.first - 1; x <= cell.first + 1; x++)
            {
                for (int y = cell.second - 1; y <= cell.second + 1; y++)
                {
                    auto neighbor = cells.find({x, y});

                    if (neighbor == cells.end())
                    {
                        continue;
                    }

                    for (size_t j : neighbor->second)
                    {
                        const Overlap& other = overlaps[j];
                        bool lower_contiguous = overlap.lower == other.lower || platforms[overlap.lower].neighbors.contains(other.lower);
                        bool upper_contiguous = overlap.upper == other.upper || platforms[overlap.upper].neighbors.contains(other.upper);

                        if (!lower_contiguous || !upper_contiguous || !overlap.bounds.Overlaps(other.bounds) ||
                            IntersectPolygons(overlap.polygon, other.polygon).empty())
                        {
                            continue;
                        }

                        size_t root_a = Root(roots, i);
                        size_t root_b = Root(roots, j);

                        roots[std::max(root_a, root_b)] = std::min(root_a, root_b);
                    }
                }
            }

            cells[cell].push_back(i);
        }

        std::vector<double> areas(overlaps.size());

        for (const auto& [cell, indices] : cells)
        {
            if (IsCanceled(token))
            {
                return;
            }

            std::map<size_t, std::vector<std::vector<RadarMapVertex>>> covered;

            for (size_t index : indices)
            {
                size_t root = Root(roots, index);
                std::vector<std::vector<RadarMapVertex>>& region = covered[root];
                std::vector<std::vector<RadarMapVertex>> pieces{overlaps[index].polygon};

                for (const std::vector<RadarMapVertex>& polygon : region)
                {
                    std::vector<std::vector<RadarMapVertex>> remaining;

                    for (const std::vector<RadarMapVertex>& piece : pieces)
                    {
                        std::vector<std::vector<RadarMapVertex>> fragments = SubtractFootprint(piece, polygon);

                        for (std::vector<RadarMapVertex>& fragment : fragments)
                        {
                            remaining.push_back(std::move(fragment));
                        }
                    }

                    pieces = std::move(remaining);

                    if (pieces.empty())
                    {
                        break;
                    }
                }

                for (std::vector<RadarMapVertex>& piece : pieces)
                {
                    areas[root] += std::abs(SignedArea(piece));
                    region.push_back(std::move(piece));
                }
            }
        }

        for (size_t i = 0; i < areas.size(); i++)
        {
            if (IsCanceled(token))
            {
                return;
            }

            analysis.largest_overlap_area = std::max(analysis.largest_overlap_area, (float)areas[i]);

            if (areas[i] + kGeometryEpsilon >= kSignificantOverlapArea)
            {
                analysis.significant_overlap_regions++;
            }
        }

        for (size_t i = 0; i < overlaps.size(); i++)
        {
            if (IsCanceled(token))
            {
                return;
            }

            if (areas[Root(roots, i)] + kGeometryEpsilon >= kSignificantOverlapArea)
            {
                const Overlap& overlap = overlaps[i];
                platforms[overlap.lower].conflicts.insert(overlap.upper);
                platforms[overlap.upper].conflicts.insert(overlap.lower);
            }
        }
    }

    std::vector<Platform> Platforms(
        const RadarMapGeometry& geometry,
        const std::vector<Candidate>& candidates,
        RadarLayerAnalysis& analysis,
        const taskcoro::CancellationToken* token
    )
    {
        std::map<std::pair<int, int>, std::vector<size_t>> cells;

        for (size_t i = 0; i < candidates.size(); i++)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            const Candidate& candidate = candidates[i];
            cells[{Cell(candidate.center.x), Cell(candidate.center.y)}].push_back(i);
        }

        std::set<std::pair<size_t, size_t>> pairs;

        for (const auto& [cell, indices] : cells)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            for (int x = cell.first - 1; x <= cell.first + 1; x++)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                for (int y = cell.second - 1; y <= cell.second + 1; y++)
                {
                    auto neighbor = cells.find({x, y});

                    if (neighbor == cells.end())
                    {
                        continue;
                    }

                    for (size_t a : indices)
                    {
                        for (size_t b : neighbor->second)
                        {
                            if (a < b)
                            {
                                pairs.emplace(a, b);
                            }
                        }
                    }
                }
            }
        }

        std::vector<size_t> roots(candidates.size());
        std::iota(roots.begin(), roots.end(), 0u);
        std::vector<std::pair<size_t, size_t>> neighbors;
        std::vector<Overlap> overlaps;

        for (const auto& [a, b] : pairs)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            if (!candidates[a].bounds.Overlaps(candidates[b].bounds))
            {
                continue;
            }

            const RadarMapSurface& first = candidates[a].surface;
            const RadarMapSurface& second = candidates[b].surface;
            std::vector<RadarMapVertex> intersection = IntersectPolygons(first.vertices, second.vertices);

            if (intersection.empty())
            {
                continue;
            }

            if (Coplanar(first, second))
            {
                size_t root_a = Root(roots, a);
                size_t root_b = Root(roots, b);

                roots[std::max(root_a, root_b)] = std::min(root_a, root_b);
                continue;
            }

            if (std::abs(SignedArea(intersection)) > kMinArea)
            {
                std::vector<std::vector<RadarMapVertex>> clear = ClearIntersection(geometry, first, {intersection}, token);
                clear = ClearIntersection(geometry, second, std::move(clear), token);

                for (std::vector<RadarMapVertex>& polygon : clear)
                {
                    ncl_math::Vector3 center = Centroid(polygon);
                    bool first_lower = Height(first, {center.x, center.y}) < Height(second, {center.x, center.y});

                    ncl_math::Aabb2 bounds = Bounds(polygon);

                    overlaps.push_back({first_lower ? a : b, first_lower ? b : a, std::move(polygon), bounds});

                }

                continue;
            }

            for (const RadarMapVertex& vertex : intersection)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                if (std::abs(vertex.pos.z - Height(second, {vertex.pos.x, vertex.pos.y})) <= kMergedSpan)
                {
                    neighbors.emplace_back(a, b);
                    break;
                }
            }
        }

        std::map<size_t, size_t> platform_roots;
        std::vector<Platform> platforms;
        std::vector<size_t> owners(candidates.size());

        for (size_t i = 0; i < candidates.size(); i++)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            size_t root = Root(roots, i);
            auto [entry, inserted] = platform_roots.emplace(root, platforms.size());

            if (inserted)
            {
                platforms.emplace_back();
            }

            size_t owner = entry->second;
            owners[i] = owner;

            Platform& platform = platforms[owner];
            const Candidate& candidate = candidates[i];

            platform.candidates.push_back(i);
            platform.height += candidate.center.z * candidate.area;
            platform.area += candidate.area;
            platform.minimum = std::min(platform.minimum, candidate.minimum);
            platform.maximum = std::max(platform.maximum, candidate.maximum);
            platform.bounds.Include(candidate.bounds.min);
            platform.bounds.Include(candidate.bounds.max);
            platform.surface_id = std::min(platform.surface_id, candidate.surface.surface_id);
        }

        for (Platform& platform : platforms)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            platform.height /= platform.area;
        }

        for (const auto& [a, b] : neighbors)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            platforms[owners[a]].neighbors.insert(owners[b]);
            platforms[owners[b]].neighbors.insert(owners[a]);
        }

        for (Overlap& overlap : overlaps)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            overlap.lower = owners[overlap.lower];
            overlap.upper = owners[overlap.upper];
        }

        SignificantConflicts(platforms, overlaps, analysis, token);
        return platforms;
    }

    bool Conflicts(const Group& group, const Platform& platform)
    {
        return std::any_of(group.platforms.begin(), group.platforms.end(), [&](size_t member) {
            return platform.conflicts.contains(member);
        });

    }

    bool Compatible(const Group& group, const Platform& platform)
    {
        return !Conflicts(group, platform);
    }

    bool TransitionConflicts(const PendingTransition& pending, const std::vector<Platform>& platforms, const std::vector<Group>& groups)
    {
        std::unordered_set<size_t> members(pending.platforms.begin(), pending.platforms.end());

        for (size_t member : pending.platforms)
        {
            const Platform& platform = platforms[member];

            for (size_t conflict : platform.conflicts)
            {
                if (members.contains(conflict))
                {
                    return true;
                }
            }

            for (int endpoint : pending.endpoints)
            {
                if (Conflicts(groups[endpoint], platform))
                {
                    return true;
                }
            }
        }

        return false;
    }

    void AddPlatform(Group& group, Platform& platform, size_t index, int group_index, const std::string& reason)
    {
        if (!platform.conflicts.empty())
        {
            group.height = (group.height * group.height_area + platform.height * platform.area) / (group.height_area + platform.area);
            group.height_area += platform.area;
        }
        else if (group.height_area == 0.0f)
        {
            group.height = (group.height * group.area + platform.height * platform.area) / (group.area + platform.area);
        }

        group.area += platform.area;
        group.minimum = std::min(group.minimum, platform.minimum);
        group.maximum = std::max(group.maximum, platform.maximum);
        group.bounds.Include(platform.bounds.min);
        group.bounds.Include(platform.bounds.max);
        group.surface_id = std::min(group.surface_id, platform.surface_id);
        group.platforms.push_back(index);

        if (std::find(group.reasons.begin(), group.reasons.end(), reason) == group.reasons.end())
        {
            group.reasons.push_back(reason);
        }

        platform.group = group_index;
    }

    void AssignPlatform(std::vector<Group>& groups, std::vector<Platform>& platforms, size_t index)
    {
        Platform& platform = platforms[index];
        int group_index = -1;
        float best_distance = kUnbounded;
        bool best_neighbor = false;

        for (size_t i = 0; i < groups.size(); i++)
        {
            if (!Compatible(groups[i], platform))
            {
                continue;
            }

            bool neighbor = std::any_of(platform.neighbors.begin(), platform.neighbors.end(), [&](size_t member) {
                return platforms[member].group == (int)i;
            });

            float distance = std::abs(groups[i].height - platform.height);

            if (group_index < 0 || std::tuple{!neighbor, distance, i} < std::tuple{!best_neighbor, best_distance, (size_t)group_index})
            {
                best_neighbor = neighbor;
                best_distance = distance;
                group_index = (int)i;
            }
        }

        if (group_index < 0)
        {
            group_index = (int)groups.size();
            groups.emplace_back();
            AddPlatform(groups.back(), platform, index, group_index, "accessible area");
        }
        else
        {
            AddPlatform(groups[group_index], platform, index, group_index, "compatible area");
        }
    }

    bool HasArea(std::span<const RadarMapVertex> polygon)
    {
        if (polygon.size() < 3)
        {
            return false;
        }

        for (size_t i = 1; i + 1 < polygon.size(); i++)
        {
            if ((polygon[i].pos - polygon[0].pos).Cross(polygon[i + 1].pos - polygon[0].pos).LengthSqr() > kMinArea * kMinArea)
            {
                return true;
            }
        }

        return false;
    }

    std::vector<RadarMapSurface> SubtractHide(
        const RadarMapSurface& surface,
        const ncl_math::Aabb3& bounds,
        const taskcoro::CancellationToken* token
    )
    {
        for (int axis = 0; axis < 3; axis++)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            float minimum = kUnbounded;
            float maximum = -kUnbounded;

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                minimum = std::min(minimum, vertex.pos[axis]);
                maximum = std::max(maximum, vertex.pos[axis]);
            }

            if (maximum < bounds.min[axis] || minimum >= bounds.max[axis])
            {
                return {surface};
            }
        }

        RadarMapSurface inside = surface;
        std::vector<RadarMapSurface> outside;

        for (int axis = 0; axis < 3; axis++)
        {
            for (int side = 0; side < 2; side++)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                ncl_math::Vector3 normal{};
                normal[axis] = side == 0 ? 1.0f : -1.0f;
                float offset = side == 0 ? bounds.min[axis] : -bounds.max[axis];
                PolygonPlane plane{normal, {}, offset};
                float minimum = kUnbounded;
                float maximum = -kUnbounded;

                for (const RadarMapVertex& vertex : inside.vertices)
                {
                    float distance = plane.normal.Dot(vertex.pos) - plane.offset;
                    minimum = std::min(minimum, distance);
                    maximum = std::max(maximum, distance);
                }

                if (minimum >= 0.0f)
                {
                    continue;
                }

                if (maximum <= 0.0f)
                {
                    outside.push_back(std::move(inside));
                    return outside;
                }

                RadarMapSurface piece = FragmentSurface(surface, ClipPlane(inside.vertices, plane, -1.0f));

                if (HasArea(piece.vertices))
                {
                    outside.push_back(std::move(piece));
                }

                inside.vertices = ClipPlane(inside.vertices, plane);
            }
        }

        return outside;
    }

    bool ApplyHides(
        std::vector<std::vector<RadarMapSurface>>& meshes,
        std::span<const ncl_math::Aabb3> hides,
        const taskcoro::CancellationToken* token
    )
    {
        for (std::vector<RadarMapSurface>& mesh : meshes)
        {
            for (const ncl_math::Aabb3& bounds : hides)
            {
                if (IsCanceled(token))
                {
                    return false;
                }

                std::vector<RadarMapSurface> visible;

                for (const RadarMapSurface& surface : mesh)
                {
                    if (IsCanceled(token))
                    {
                        return false;
                    }

                    std::vector<RadarMapSurface> pieces = SubtractHide(surface, bounds, token);

                    for (RadarMapSurface& piece : pieces)
                    {
                        visible.push_back(std::move(piece));
                    }
                }

                mesh = std::move(visible);
            }
        }

        return !IsCanceled(token);
    }

    std::vector<std::vector<RadarMapVertex>> RemoveAboveSupport(
        std::span<const RadarMapVertex> polygon,
        const RadarMapSurface& support,
        const RadarMapSurface& upper
    )
    {
        if (Coplanar(support, upper) || !Bounds(polygon).Overlaps(Bounds(upper.vertices)))
        {
            return {{polygon.begin(), polygon.end()}};
        }

        const ncl_math::Vector3& origin = support.vertices[0].pos;
        PolygonPlane higher{
            {support.normal.x / support.normal.z - upper.normal.x / upper.normal.z,
             support.normal.y / support.normal.z - upper.normal.y / upper.normal.z,
             0.0f},
            origin,
            origin.z - Height(upper, {origin.x, origin.y}),
        };

        if (std::none_of(polygon.begin(), polygon.end(), [&](const RadarMapVertex& vertex) {
                return higher.normal.Dot(vertex.pos - higher.origin) - higher.offset > kGeometryEpsilon;
            }))
        {
            return {{polygon.begin(), polygon.end()}};
        }

        std::vector<PolygonPlane> planes;
        float direction = SignedArea(upper.vertices) >= 0.0f ? 1.0f : -1.0f;

        for (size_t i = 0; i < upper.vertices.size(); i++)
        {
            const ncl_math::Vector3& a = upper.vertices[i].pos;
            const ncl_math::Vector3& b = upper.vertices[(i + 1) % upper.vertices.size()].pos;
            ncl_math::Vector2 normal = ncl_math::Vector2{b.x - a.x, b.y - a.y}.Perp() * direction;
            planes.push_back({{normal.x, normal.y, 0.0f}, a, 0.0f});

        }

        planes.push_back(higher);
        planes.push_back({upper.normal, upper.vertices[0].pos, 0.0f});

        std::vector<std::vector<RadarMapVertex>> outside;
        std::vector<RadarMapVertex> inside(polygon.begin(), polygon.end());

        // Subtract the upper footprint's prism, retaining the pieces outside each half-space.
        for (const PolygonPlane& plane : planes)
        {
            float minimum = kUnbounded;
            float maximum = -kUnbounded;

            for (const RadarMapVertex& vertex : inside)
            {
                float distance = plane.normal.Dot(vertex.pos - plane.origin) - plane.offset;
                minimum = std::min(minimum, distance);
                maximum = std::max(maximum, distance);
            }

            if (minimum >= 0.0f)
            {
                continue;
            }

            if (maximum <= 0.0f)
            {
                outside.push_back(std::move(inside));
                break;
            }

            std::vector<RadarMapVertex> piece = ClipPlane(inside, plane, -1.0f);

            if (HasArea(piece))
            {
                outside.push_back(std::move(piece));
            }

            inside = ClipPlane(inside, plane);

            if (!HasArea(inside))
            {
                break;
            }
        }

        return outside;
    }

    ncl_math::Vector3 ClosestPoint(const RadarMapSurface& surface, ncl_math::Vector3 position)
    {
        const ncl_math::Vector3& origin = surface.vertices[0].pos;
        ncl_math::Vector3 projected = position - surface.normal * surface.normal.Dot(position - origin);

        if (Contains(surface.vertices, {projected.x, projected.y}))
        {
            return projected;
        }

        ncl_math::Vector3 closest{};
        float best = kUnbounded;

        for (size_t i = 0; i < surface.vertices.size(); i++)
        {
            ncl_math::Vector3 a = surface.vertices[i].pos;
            ncl_math::Vector3 b = surface.vertices[(i + 1) % surface.vertices.size()].pos;
            ncl_math::Vector3 point = ncl_math::ClosestPointOnSegment(a, b, position);
            float distance = (point - position).LengthSqr();

            if (distance < best)
            {
                best = distance;
                closest = point;
            }
        }

        return closest;
    }

    std::vector<RenderSkyFace> RenderSkies(const RadarMapGeometry& geometry)
    {
        std::vector<RenderSkyFace> skies;

        for (const RadarMapSurface& surface : geometry.surfaces)
        {
            if (!surface.sky || std::abs(surface.normal.z) < 0.5f || surface.vertices.empty())
            {
                continue;
            }

            float height = 0.0f;

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                height += vertex.pos.z;
            }

            skies.push_back({Bounds(surface.vertices), height / (float)surface.vertices.size(), surface.vertices});
        }

        return skies;
    }

    bool IsRenderRoof(const RadarMapSurface& surface, std::span<const RenderSkyFace> skies)
    {
        ncl_math::Vector2 center{};
        float top = -kUnbounded;

        for (const RadarMapVertex& vertex : surface.vertices)
        {
            center = center + ncl_math::Vector2{vertex.pos.x, vertex.pos.y};
            top = std::max(top, vertex.pos.z);
        }

        center = center / (float)surface.vertices.size();

        for (const RenderSkyFace& sky : skies)
        {
            if (sky.bounds.Contains(center) && Contains(sky.footprint, center) && sky.height >= top - kSkyRoofTolerance &&
                sky.height <= top + kSkyRoofGap)
            {
                return true;
            }
        }

        return false;
    }

    std::vector<RadarLayerSupport> RestoreRenderSupports(
        const RadarMapGeometry& geometry,
        std::span<const RadarLayerSupport> supports,
        const RadarLayerOverrides& overrides,
        std::span<const int> override_ids,
        std::span<const RenderSkyFace> skies,
        std::span<const RadarLayerTransition> transitions,
        const taskcoro::CancellationToken* token
    )
    {
        std::map<int, std::vector<const RadarLayerSupport*>> sources;

        for (const RadarLayerSupport& support : supports)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            sources[support.surface.surface_id].push_back(&support);
        }

        std::vector<RadarLayerSupport> restored;

        for (const RadarMapSurface& surface : geometry.surfaces)
        {
            if (IsCanceled(token))
            {
                return {};
            }

            auto source = sources.find(surface.surface_id);

            if (source == sources.end() || IsRenderRoof(surface, skies))
            {
                continue;
            }

            const RadarLayerSupport* constant_owner = source->second.front();
            std::map<int, std::pair<float, float>> heights;

            for (const RadarLayerSupport* support : source->second)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                if (support->layer != source->second.front()->layer || support->automatic_layer != source->second.front()->automatic_layer)
                {
                    constant_owner = nullptr;
                }

                auto entry = heights.try_emplace(support->layer, kUnbounded, -kUnbounded).first;

                for (const RadarMapVertex& vertex : support->surface.vertices)
                {
                    entry->second.first = std::min(entry->second.first, vertex.pos.z);
                    entry->second.second = std::max(entry->second.second, vertex.pos.z);
                }
            }

            std::set<float> boundaries;

            if (heights.size() > 1)
            {
                for (const auto& [layer, range] : heights)
                {
                    boundaries.insert(range.first);
                    boundaries.insert(range.second);
                }
            }

            for (const RadarLayerTransition& transition : transitions)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                if (std::find(transition.surface_ids.begin(), transition.surface_ids.end(), surface.surface_id) !=
                    transition.surface_ids.end())
                {
                    boundaries.insert(transition.height);
                }
            }

            ncl_math::Aabb2 bounds = Bounds(surface.vertices);

            for (int x = Cell(bounds.min.x); x <= Cell(bounds.max.x); x++)
            {
                if (IsCanceled(token))
                {
                    return {};
                }

                for (int y = Cell(bounds.min.y); y <= Cell(bounds.max.y); y++)
                {
                    RadarMapSurface cell = FragmentSurface(
                        surface,
                        MapGeometry_Clip(
                            surface.vertices,
                            {{x * kCellSize, y * kCellSize, -kUnbounded}, {(x + 1) * kCellSize, (y + 1) * kCellSize, kUnbounded}}
                        )
                    );

                    if (std::abs(SignedArea(cell.vertices)) <= kMinArea)
                    {
                        continue;
                    }

                    std::vector<RadarMapSurface> pieces = SplitOverrides(cell, overrides);

                    for (float boundary : boundaries)
                    {
                        std::vector<RadarMapSurface> split;

                        for (const RadarMapSurface& piece : pieces)
                        {
                            float minimum = kUnbounded;
                            float maximum = -kUnbounded;

                            for (const RadarMapVertex& vertex : piece.vertices)
                            {
                                minimum = std::min(minimum, vertex.pos.z);
                                maximum = std::max(maximum, vertex.pos.z);
                            }

                            if (minimum < boundary && maximum > boundary)
                            {
                                for (float direction : {-1.0f, 1.0f})
                                {
                                    RadarMapSurface part =
                                        FragmentSurface(piece, ClipPlane(piece.vertices, {{0.0f, 0.0f, 1.0f}, {}, boundary}, direction));
                                    split.push_back(std::move(part));
                                }
                            }
                            else
                            {
                                split.push_back(piece);
                            }
                        }

                        pieces = std::move(split);
                    }

                    for (RadarMapSurface& piece : pieces)
                    {
                        ncl_math::Vector3 center = Centroid(piece.vertices);
                        int assigned = OverrideLayer(center, overrides, override_ids);
                        int automatic = -1;
                        float boundary_distance = kUnbounded;

                        for (const RadarLayerTransition& transition : transitions)
                        {
                            if (std::find(transition.surface_ids.begin(), transition.surface_ids.end(), surface.surface_id) ==
                                transition.surface_ids.end())
                            {
                                continue;
                            }

                            float current = std::abs(center.z - transition.height);

                            if (current < boundary_distance)
                            {
                                automatic = center.z <= transition.height ? transition.lower : transition.upper;
                                boundary_distance = current;
                            }
                        }

                        const RadarLayerSupport* owner = constant_owner;
                        float distance = kUnbounded;

                        for (const RadarLayerSupport* support : source->second)
                        {
                            if (constant_owner)
                            {
                                break;
                            }

                            float current = (ClosestPoint(support->surface, center) - center).LengthSqr();

                            if (current < distance)
                            {
                                owner = support;
                                distance = current;
                            }
                        }

                        if (owner && assigned != kUnavailableOverride)
                        {
                            int automatic_layer = boundary_distance < kUnbounded ? automatic : owner->automatic_layer;
                            int layer = assigned >= 0 ? assigned : automatic_layer;

                            if (layer >= 0)
                            {
                                restored.push_back({std::move(piece), layer, automatic_layer});

                            }
                        }
                    }
                }
            }
        }

        return restored;
    }

    std::vector<std::vector<RadarMapVertex>> SubtractRenderFootprint(
        std::span<const RadarMapVertex> polygon,
        std::span<const RadarMapVertex> footprint
    )
    {
        std::vector<std::vector<RadarMapVertex>> outside;
        std::vector<RadarMapVertex> inside(polygon.begin(), polygon.end());
        float direction = SignedArea(footprint) >= 0.0f ? 1.0f : -1.0f;

        for (size_t i = 0; i < footprint.size() && HasArea(inside); i++)
        {
            const ncl_math::Vector3& a = footprint[i].pos;
            const ncl_math::Vector3& b = footprint[(i + 1) % footprint.size()].pos;
            ncl_math::Vector2 normal = ncl_math::Vector2{b.x - a.x, b.y - a.y}.Perp() * direction;

            if (normal.LengthSqr() == 0.0f)
            {
                continue;
            }

            PolygonPlane plane{{normal.x, normal.y, 0.0f}, a, 0.0f};
            float minimum = kUnbounded;
            float maximum = -kUnbounded;

            for (const RadarMapVertex& vertex : inside)
            {
                float distance = normal.Dot(ncl_math::Vector2{vertex.pos.x - a.x, vertex.pos.y - a.y});

                minimum = std::min(minimum, distance);
                maximum = std::max(maximum, distance);
            }

            if (minimum >= 0.0f)
            {
                continue;
            }

            if (maximum <= 0.0f)
            {
                outside.push_back(std::move(inside));
                break;
            }

            std::vector<RadarMapVertex> piece = ClipPlane(inside, plane, -1.0f);

            if (HasArea(piece))
            {
                outside.push_back(std::move(piece));
            }

            inside = ClipPlane(inside, plane);
        }

        return outside;
    }

    std::vector<std::vector<RadarMapVertex>> SplitDetailOverrides(
        std::span<const RadarMapVertex> polygon,
        const DetailOwner& support,
        const RadarLayerOverrides& overrides
    )
    {
        std::vector<PolygonPlane> planes;
        const ncl_math::Vector3& origin = support.point;

        auto add_height = [&](float boundary) {
            planes.push_back(
                {{-support.normal.x / support.normal.z, -support.normal.y / support.normal.z, 0.0f}, origin, boundary - origin.z}
            );
        };

        for (const RadarLayerOverride& layer : overrides.layers)
        {
            add_height(layer.min_z);
            add_height(layer.max_z);
        }

        for (const RadarLayerRegion& region : overrides.regions)
        {
            for (int axis = 0; axis < 2; axis++)
            {
                ncl_math::Vector3 normal{};
                normal[axis] = 1.0f;
                planes.push_back({normal, {}, region.bounds.min[axis]});

                planes.push_back({normal, {}, region.bounds.max[axis]});

            }

            add_height(region.bounds.min.z);
            add_height(region.bounds.max.z);
        }

        std::vector<std::vector<RadarMapVertex>> pieces{{polygon.begin(), polygon.end()}};

        for (const PolygonPlane& plane : planes)
        {
            std::vector<std::vector<RadarMapVertex>> split;

            for (const std::vector<RadarMapVertex>& piece : pieces)
            {
                float minimum = kUnbounded;
                float maximum = -kUnbounded;

                for (const RadarMapVertex& vertex : piece)
                {
                    float distance = plane.normal.Dot(vertex.pos - plane.origin) - plane.offset;
                    minimum = std::min(minimum, distance);
                    maximum = std::max(maximum, distance);
                }

                if (minimum < 0.0f && maximum > 0.0f)
                {
                    for (float direction : {-1.0f, 1.0f})
                    {
                        std::vector<RadarMapVertex> part = ClipPlane(piece, plane, direction);

                        if (HasArea(part))
                        {
                            split.push_back(std::move(part));
                        }
                    }
                }
                else
                {
                    split.push_back(piece);
                }
            }

            pieces = std::move(split);
        }

        return pieces;
    }
} // namespace

RadarMapStatus RadarMapLayers::Build(RadarMapGeometry geometry, const taskcoro::CancellationToken* token)
{
    if (BuildAutomatic(std::move(geometry), token) == RadarMapStatus::Ready)
    {
        Rebuild(token);
        if (IsCanceled(token))
        {
            Reset();
        }
    }
    return get_analysis().status;
}

RadarMapStatus RadarMapLayers::Build(
    RadarMapGeometry geometry,
    const RadarLayerOverrides& overrides,
    std::string& warning,
    const taskcoro::CancellationToken* token
)
{
    warning.clear();
    if (BuildAutomatic(std::move(geometry), token) == RadarMapStatus::Ready && !ApplyOverrides(overrides, warning, token))
    {
        if (!IsCanceled(token))
        {
            Rebuild(token);
        }
    }
    if (IsCanceled(token))
    {
        Reset();
    }
    return get_analysis().status;
}

RadarMapStatus RadarMapLayers::BuildAutomatic(RadarMapGeometry geometry, const taskcoro::CancellationToken* token)
{
    Reset();

    if (IsCanceled(token))
    {
        return RadarMapStatus::Waiting;
    }

    std::shared_ptr<RadarAutomaticLayout> automatic = std::make_shared<RadarAutomaticLayout>();
    automatic->geometry = std::move(geometry);
    automatic_ = automatic;
    const RadarMapGeometry& source_geometry = automatic->geometry;
    RadarLayerLayout& layout = automatic->layout;

    layout.analysis.reachability = MapReachability_Analyze(source_geometry, token);

    std::vector<Candidate> candidates = Collect(source_geometry, layout.analysis.reachability.excluded_surfaces, token);

    if (IsCanceled(token))
    {
        Reset();
        return layout.analysis.status;
    }

    if (candidates.empty())
    {
        layout.analysis.status = RadarMapStatus::Failed;
        return layout.analysis.status;
    }

    layout.analysis.overlap_area_threshold = kSignificantOverlapArea;

    std::vector<Platform> platforms = Platforms(source_geometry, candidates, layout.analysis, token);

    if (IsCanceled(token))
    {
        Reset();
        return layout.analysis.status;
    }

    std::vector<size_t> order(platforms.size());
    std::iota(order.begin(), order.end(), 0u);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const Platform& first = platforms[a];
        const Platform& second = platforms[b];
        return std::tuple{first.height, -first.area, first.bounds.min.x, first.bounds.min.y, first.surface_id} <
               std::tuple{second.height, -second.area, second.bounds.min.x, second.bounds.min.y, second.surface_id};
    });

    std::vector<Group> groups;

    for (size_t index : order)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        Platform& platform = platforms[index];

        if (platform.conflicts.empty())
        {
            continue;
        }

        int group_index = -1;

        for (size_t i = 0; i < groups.size(); i++)
        {
            if (Compatible(groups[i], platform))
            {
                group_index = (int)i;
                break;
            }
        }

        if (group_index < 0)
        {
            group_index = (int)groups.size();
            groups.emplace_back();
        }

        AddPlatform(groups[group_index], platform, index, group_index, "significant overlap region");
    }

    std::vector<PendingTransition> pending_transitions;
    std::vector<bool> visited(platforms.size());

    for (size_t start : order)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        if (visited[start] || platforms[start].group >= 0)
        {
            continue;
        }

        std::vector<size_t> component{start};
        std::set<int> endpoints;
        std::set<size_t> endpoint_platforms;
        visited[start] = true;

        for (size_t i = 0; i < component.size(); i++)
        {
            for (size_t neighbor : platforms[component[i]].neighbors)
            {
                if (platforms[neighbor].group >= 0)
                {
                    endpoints.insert(platforms[neighbor].group);
                    endpoint_platforms.insert(neighbor);
                }
                else if (!visited[neighbor])
                {
                    visited[neighbor] = true;
                    component.push_back(neighbor);
                }
            }
        }

        if (endpoints.size() < 2)
        {
            continue;
        }

        PendingTransition pending{std::move(component), {endpoints.begin(), endpoints.end()}, {}};
        std::map<int, float> weights;

        for (size_t neighbor : endpoint_platforms)
        {
            const Platform& platform = platforms[neighbor];
            pending.heights[platform.group] += platform.height * platform.area;
            weights[platform.group] += platform.area;
        }

        for (int endpoint : pending.endpoints)
        {
            pending.heights[endpoint] /= weights[endpoint];
        }

        if (TransitionConflicts(pending, platforms, groups))
        {
            continue;
        }

        for (size_t member : pending.platforms)
        {
            platforms[member].group = -2;
        }

        pending_transitions.push_back(std::move(pending));
    }

    for (size_t index : order)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        if (platforms[index].group == -1)
        {
            AssignPlatform(groups, platforms, index);
        }
    }

    bool rejected = true;

    while (rejected)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        rejected = false;

        for (PendingTransition& pending : pending_transitions)
        {
            if (pending.platforms.empty() || !TransitionConflicts(pending, platforms, groups))
            {
                continue;
            }

            std::unordered_set<size_t> members(pending.platforms.begin(), pending.platforms.end());

            for (size_t index : order)
            {
                if (members.contains(index))
                {
                    AssignPlatform(groups, platforms, index);
                }
            }

            pending.platforms.clear();
            rejected = true;
        }
    }

    for (const PendingTransition& pending : pending_transitions)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        if (pending.platforms.empty())
        {
            continue;
        }

        std::vector<int> sorted_endpoints = pending.endpoints;
        std::sort(sorted_endpoints.begin(), sorted_endpoints.end(), [&](int a, int b) {
            return std::tuple{pending.heights.at(a), a} < std::tuple{pending.heights.at(b), b};
        });

        std::set<int> source_ids;

        for (size_t member : pending.platforms)
        {
            Platform& platform = platforms[member];

            for (size_t candidate_index : platform.candidates)
            {
                const RadarMapSurface& surface = candidates[candidate_index].surface;
                source_ids.insert(surface.surface_id);

                for (size_t i = 0; i < sorted_endpoints.size(); i++)
                {
                    int group_index = sorted_endpoints[i];
                    float minimum =
                        i == 0 ? -kUnbounded : (pending.heights.at(sorted_endpoints[i - 1]) + pending.heights.at(group_index)) * 0.5f;
                    float maximum = i + 1 == sorted_endpoints.size()
                                        ? kUnbounded
                                        : (pending.heights.at(group_index) + pending.heights.at(sorted_endpoints[i + 1])) * 0.5f;

                    // A horizontal tread on the boundary belongs to the lower endpoint only.
                    if (i > 0 && candidates[candidate_index].maximum <= minimum)
                    {
                        continue;
                    }

                    RadarMapSurface fragment = FragmentSurface(
                        surface,
                        MapGeometry_Clip(surface.vertices, {{-kUnbounded, -kUnbounded, minimum}, {kUnbounded, kUnbounded, maximum}})
                    );

                    if (std::abs(SignedArea(fragment.vertices)) > kMinArea)
                    {
                        layout.supports.push_back({std::move(fragment), group_index});
                    }
                }
            }
        }

        for (size_t i = 1; i < sorted_endpoints.size(); i++)
        {
            int lower = sorted_endpoints[i - 1];
            int upper = sorted_endpoints[i];
            layout.analysis.transitions.push_back(
                {lower,
                 upper,
                 (pending.heights.at(lower) + pending.heights.at(upper)) * 0.5f,
                 {source_ids.begin(), source_ids.end()},
                 pending.heights.at(upper) - pending.heights.at(lower)}
            );
        }
    }

    for (const Platform& platform : platforms)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        if (platform.group >= 0)
        {
            for (size_t index : platform.candidates)
            {
                layout.supports.push_back({std::move(candidates[index].surface), platform.group});
            }
        }
    }

    std::vector<size_t> group_order(groups.size());
    std::iota(group_order.begin(), group_order.end(), 0u);
    std::sort(group_order.begin(), group_order.end(), [&](size_t a, size_t b) {
        const Group& first = groups[a];
        const Group& second = groups[b];
        return std::tuple{first.height, -first.area, first.bounds.min.x, first.bounds.min.y, first.surface_id} <
               std::tuple{second.height, -second.area, second.bounds.min.x, second.bounds.min.y, second.surface_id};
    });

    std::vector<int> ids(groups.size());

    for (size_t id = 0; id < group_order.size(); id++)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        size_t index = group_order[id];
        ids[index] = (int)id;
        const Group& group = groups[index];

        layout.layers.push_back({(int)id, "Layer " + std::to_string(id + 1), group.height, 0.0f});

        layout.analysis.layers.push_back({(int)id, 0.0f, ncl_math::Aabb2::Empty(), kUnbounded, -kUnbounded, group.reasons});
    }

    for (RadarLayerSupport& support : layout.supports)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        support.layer = ids[support.layer];
        support.automatic_layer = support.layer;
        RadarLayerGroupAnalysis& analysis = layout.analysis.layers[support.layer];
        float area = std::abs(SignedArea(support.surface.vertices));
        analysis.area += area;
        layout.layers[support.layer].area += area;

        for (const RadarMapVertex& vertex : support.surface.vertices)
        {
            analysis.bounds.Include({vertex.pos.x, vertex.pos.y});

            analysis.min_height = std::min(analysis.min_height, vertex.pos.z);
            analysis.max_height = std::max(analysis.max_height, vertex.pos.z);
        }
    }

    for (RadarLayerTransition& transition : layout.analysis.transitions)
    {
        if (IsCanceled(token))
        {
            Reset();
            return layout.analysis.status;
        }

        transition.lower = ids[transition.lower];
        transition.upper = ids[transition.upper];

        for (int layer : {transition.lower, transition.upper})
        {
            std::vector<std::string>& reasons = layout.analysis.layers[layer].reasons;

            if (std::find(reasons.begin(), reasons.end(), "transition partition") == reasons.end())
            {
                reasons.push_back("transition partition");
            }
        }
    }

    layout.analysis.status = RadarMapStatus::Ready;
    return layout.analysis.status;
}

bool RadarMapLayers::ApplyOverrides(const RadarLayerOverrides& overrides, std::string& error, const taskcoro::CancellationToken* token)
{
    if (IsCanceled(token))
    {
        error = "Map analysis was canceled.";
        return false;
    }

    if (!MapLayerOverrides_Validate(overrides, error))
    {
        return false;
    }

    RadarMapLayers corrected;

    if (!automatic_ || automatic_->layout.analysis.status != RadarMapStatus::Ready)
    {
        error = IsCanceled(token) ? "Map analysis was canceled." : "Map has no accessible support geometry.";
        return false;
    }

    corrected.automatic_ = automatic_;

    if (overrides.layers.empty())
    {
        corrected.overrides_ = overrides;
        corrected.Rebuild(token);

        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        *this = std::move(corrected);
        error.clear();
        return true;
    }

    corrected.corrected_layout_.emplace();
    RadarLayerLayout& layout = *corrected.corrected_layout_;
    layout.analysis = automatic_->layout.analysis;

    size_t automatic_count = automatic_->layout.layers.size();
    std::vector<RadarLayerInfo> provisional = automatic_->layout.layers;

    for (RadarLayerInfo& layer : provisional)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        layer.height = 0.0f;
        layer.area = 0.0f;
    }

    std::vector<int> override_ids;

    for (const RadarLayerOverride& layer : overrides.layers)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        int id = (int)provisional.size();
        override_ids.push_back(id);
        provisional.push_back({id, layer.name, 0.0f, 0.0f});

    }

    std::vector<RadarLayerSupport> assigned;

    for (const RadarLayerSupport& support : automatic_->layout.supports)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        std::vector<RadarMapSurface> pieces = SplitOverrides(support.surface, overrides);

        for (RadarMapSurface& piece : pieces)
        {
            int layer = OverrideLayer(Centroid(piece.vertices), overrides, override_ids);
            assigned.push_back({std::move(piece), layer >= 0 ? layer : support.layer, support.layer});

        }
    }

    std::vector<RadarLayerGroupAnalysis> groups(provisional.size());

    for (size_t i = 0; i < automatic_count; i++)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        groups[i].reasons = layout.analysis.layers[i].reasons;
    }

    for (size_t i = automatic_count; i < groups.size(); i++)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        groups[i].reasons.push_back("manual correction");
    }

    for (RadarLayerGroupAnalysis& group : groups)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        group.min_height = kUnbounded;
        group.max_height = -kUnbounded;
    }

    for (const RadarLayerSupport& support : assigned)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        float area = std::abs(SignedArea(support.surface.vertices));
        RadarLayerInfo& layer = provisional[support.layer];
        layer.area += area;
        layer.height += Centroid(support.surface.vertices).z * area;

        RadarLayerGroupAnalysis& group = groups[support.layer];
        group.area += area;

        for (const RadarMapVertex& vertex : support.surface.vertices)
        {
            group.bounds.Include({vertex.pos.x, vertex.pos.y});

            group.min_height = std::min(group.min_height, vertex.pos.z);
            group.max_height = std::max(group.max_height, vertex.pos.z);
        }
    }

    std::vector<size_t> order;

    for (size_t i = 0; i < provisional.size(); i++)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        if (provisional[i].area > 0.0f)
        {
            provisional[i].height /= provisional[i].area;
            order.push_back(i);
        }
    }

    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return std::tuple{provisional[a].height, -provisional[a].area, groups[a].bounds.min.x, groups[a].bounds.min.y, a} <
               std::tuple{provisional[b].height, -provisional[b].area, groups[b].bounds.min.x, groups[b].bounds.min.y, b};
    });

    std::vector<int> ids(provisional.size(), -1);

    layout.layers.clear();
    layout.analysis.layers.clear();

    for (size_t id = 0; id < order.size(); id++)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        size_t source = order[id];
        ids[source] = (int)id;
        provisional[source].id = (int)id;
        groups[source].layer = (int)id;

        layout.layers.push_back(std::move(provisional[source]));
        layout.analysis.layers.push_back(std::move(groups[source]));
    }

    for (RadarLayerSupport& support : assigned)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        support.layer = ids[support.layer];
        support.automatic_layer = ids[support.automatic_layer];
    }

    layout.supports = std::move(assigned);
    corrected.overrides_ = overrides;

    for (int id : override_ids)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        corrected.override_ids_.push_back(ids[id]);
    }

    for (RadarLayerTransition& transition : layout.analysis.transitions)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        transition.lower = ids[transition.lower];
        transition.upper = ids[transition.upper];
    }

    corrected.Rebuild(token);

    if (IsCanceled(token))
    {
        error = "Map analysis was canceled.";
        return false;
    }

    std::unordered_set<std::pair<size_t, size_t>, SupportPairHash> examined;
    std::set<std::tuple<int, int, int>> conflicts;
    std::map<std::pair<int, int>, std::vector<RadarLayerTransition>> links;

    for (const auto& [cell, indices] : corrected.index_)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        for (size_t a : indices)
        {
            for (size_t b : indices)
            {
                if (a >= b || !examined.emplace(a, b).second)
                {
                    continue;
                }

                const RadarLayerSupport& first = layout.supports[a];
                const RadarLayerSupport& second = layout.supports[b];
                std::vector<RadarMapVertex> intersection = IntersectPolygons(first.surface.vertices, second.surface.vertices);

                if (intersection.empty())
                {
                    continue;
                }

                float area = std::abs(SignedArea(intersection));

                if (area > 0.0f)
                {
                    if (first.layer == second.layer && !Coplanar(first.surface, second.surface) &&
                        std::find(corrected.override_ids_.begin(), corrected.override_ids_.end(), first.layer) !=
                            corrected.override_ids_.end())
                    {
                        int first_source = std::min(first.surface.surface_id, second.surface.surface_id);
                        int second_source = std::max(first.surface.surface_id, second.surface.surface_id);
                        conflicts.emplace(first.layer, first_source, second_source);
                    }

                    continue;
                }

                if (first.layer == second.layer)
                {
                    continue;
                }

                if (std::none_of(intersection.begin(), intersection.end(), [&](const RadarMapVertex& vertex) {
                        return std::abs(vertex.pos.z - Height(second.surface, {vertex.pos.x, vertex.pos.y})) <= kMergedSpan;
                    }))
                {
                    continue;
                }

                std::pair<int, int> pair{std::min(first.layer, second.layer), std::max(first.layer, second.layer)};
                float height = 0.0f;

                for (const RadarMapVertex& vertex : intersection)
                {
                    height += (vertex.pos.z + Height(second.surface, {vertex.pos.x, vertex.pos.y})) * 0.5f;
                }

                height /= (float)intersection.size();
                float height_span = 0.0f;

                for (const RadarLayerTransition& transition : layout.analysis.transitions)
                {
                    if (transition.lower == pair.first && transition.upper == pair.second &&
                        std::abs(transition.height - height) <= kGeometryEpsilon &&
                        (std::find(transition.surface_ids.begin(), transition.surface_ids.end(), first.surface.surface_id) !=
                             transition.surface_ids.end() ||
                         std::find(transition.surface_ids.begin(), transition.surface_ids.end(), second.surface.surface_id) !=
                             transition.surface_ids.end()))
                    {
                        height = transition.height;
                        height_span = transition.height_span;
                        break;
                    }
                }

                std::vector<RadarLayerTransition>& local = links[pair];
                auto transition = std::find_if(local.begin(), local.end(), [&](const RadarLayerTransition& entry) {
                    return std::abs(entry.height - height) <= kGeometryEpsilon;
                });

                if (transition == local.end())
                {
                    local.push_back({pair.first, pair.second, height, {}, height_span});
                    transition = local.end() - 1;
                }

                transition->surface_ids.push_back(first.surface.surface_id);
                transition->surface_ids.push_back(second.surface.surface_id);
            }
        }
    }

    for (const auto& [layer, first, second] : conflicts)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        layout.analysis.manual_conflicts.push_back(
            "Layer '" + layout.layers[layer].name + "' contains vertically overlapping supports " + std::to_string(first) + " and " +
            std::to_string(second) + "."
        );
    }

    layout.analysis.transitions.clear();

    for (auto& [pair, transitions] : links)
    {
        if (IsCanceled(token))
        {
            error = "Map analysis was canceled.";
            return false;
        }

        std::sort(transitions.begin(), transitions.end(), [](const RadarLayerTransition& a, const RadarLayerTransition& b) {
            return a.height < b.height;
        });

        for (RadarLayerTransition& transition : transitions)
        {
            std::sort(transition.surface_ids.begin(), transition.surface_ids.end());
            transition.surface_ids.erase(
                std::unique(transition.surface_ids.begin(), transition.surface_ids.end()), transition.surface_ids.end()
            );
            layout.analysis.transitions.push_back(std::move(transition));
        }
    }

    if (IsCanceled(token))
    {
        error = "Map analysis was canceled.";
        return false;
    }

    *this = std::move(corrected);
    error.clear();
    return true;
}

int RadarMapLayers::BuildLookup(size_t first, size_t count, const taskcoro::CancellationToken* token)
{
    const RadarLayerLayout& layout = get_layout();
    if (IsCanceled(token))
    {
        return -1;
    }

    RadarSupportNode node{{{kUnbounded, kUnbounded, kUnbounded}, {-kUnbounded, -kUnbounded, -kUnbounded}}, first, count};

    for (size_t i = first; i < first + count; i++)
    {
        size_t support_id = nearest_ids_[i];
        const ncl_math::Aabb3& bounds = support_bounds_[support_id];
        int layer_id = layout.supports[support_id].layer;
        auto layer_bounds = std::find_if(node.layer_bounds.begin(), node.layer_bounds.end(), [layer_id](const auto& entry) {
            return entry.first == layer_id;
        });

        if (layer_bounds == node.layer_bounds.end())
        {
            node.layer_bounds.emplace_back(layer_id, bounds);
        }
        else
        {
            layer_bounds->second.Include(bounds);
        }

        node.bounds.Include(bounds);
    }

    std::sort(node.layer_bounds.begin(), node.layer_bounds.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    int index = (int)nearest_nodes_.size();
    ncl_math::Vector3 size = node.bounds.max - node.bounds.min;

    nearest_nodes_.push_back(std::move(node));

    if (count > kLookupLeafSize)
    {
        int axis = size.y > size.x ? 1 : 0;

        if (size.z > size[axis])
        {
            axis = 2;
        }

        size_t middle = first + count / 2;
        std::nth_element(
            nearest_ids_.begin() + first, nearest_ids_.begin() + middle, nearest_ids_.begin() + first + count, [&](size_t a, size_t b) {
                const ncl_math::Aabb3& first_bounds = support_bounds_[a];
                const ncl_math::Aabb3& second_bounds = support_bounds_[b];
                return std::tuple{first_bounds.min[axis] + first_bounds.max[axis], a} <
                       std::tuple{second_bounds.min[axis] + second_bounds.max[axis], b};
            }
        );
        int lower = BuildLookup(first, middle - first, token);
        int upper = BuildLookup(middle, first + count - middle, token);

        nearest_nodes_[index].children[0] = lower;
        nearest_nodes_[index].children[1] = upper;
    }

    return index;
}

void RadarMapLayers::Rebuild(const taskcoro::CancellationToken* token)
{
    const RadarLayerLayout& layout = get_layout();
    index_.clear();
    support_bounds_.clear();
    nearest_nodes_.clear();

    nearest_ids_.resize(layout.supports.size());
    std::iota(nearest_ids_.begin(), nearest_ids_.end(), 0u);

    for (const RadarLayerSupport& support : layout.supports)
    {
        if (IsCanceled(token))
        {
            return;
        }

        ncl_math::Vector3 center = Centroid(support.surface.vertices);
        float direction = SignedArea(support.surface.vertices) >= 0.0f ? 1.0f : -1.0f;
        float scale = 1.0f;

        // Contains accepts an edge cross-product tolerance, including beyond small footprints.
        for (size_t i = 0; i < support.surface.vertices.size(); i++)
        {
            if (IsCanceled(token))
            {
                return;
            }

            ncl_math::Vector3 a = support.surface.vertices[i].pos;
            ncl_math::Vector3 b = support.surface.vertices[(i + 1) % support.surface.vertices.size()].pos;
            ncl_math::Vector2 normal = ncl_math::Vector2{b.x - a.x, b.y - a.y}.Perp() * direction;

            if (normal.LengthSqr() == 0.0f)
            {
                continue;
            }

            float distance = normal.Dot(ncl_math::Vector2{center.x - a.x, center.y - a.y});

            scale = distance > 0.0f ? std::max(scale, 1.0f + kGeometryEpsilon / distance) : kUnbounded;
        }

        ncl_math::Aabb3 bounds{center, center};

        for (const RadarMapVertex& vertex : support.surface.vertices)
        {
            if (IsCanceled(token))
            {
                return;
            }

            ncl_math::Vector3 expanded = center + (vertex.pos - center) * scale;

            bounds.Include(expanded);
        }

        support_bounds_.push_back(bounds);
    }

    if (!layout.supports.empty())
    {
        BuildLookup(0, layout.supports.size(), token);

        if (IsCanceled(token))
        {
            return;
        }
    }

    meshes_.assign(layout.layers.size(), {});

    std::map<int, std::vector<size_t>> support_owners;

    for (size_t i = 0; i < layout.supports.size(); i++)
    {
        if (IsCanceled(token))
        {
            return;
        }

        const RadarLayerSupport& support = layout.supports[i];
        support_owners[support.surface.surface_id].push_back(i);

        ncl_math::Aabb2 bounds = Bounds(support.surface.vertices);

        for (int x = Cell(bounds.min.x); x <= Cell(bounds.max.x); x++)
        {
            if (IsCanceled(token))
            {
                return;
            }

            for (int y = Cell(bounds.min.y); y <= Cell(bounds.max.y); y++)
            {
                if (IsCanceled(token))
                {
                    return;
                }

                index_[{x, y}].push_back(i);
            }
        }
    }

    std::vector<RenderSkyFace> skies = RenderSkies(automatic_->geometry);

    if (layout.layers.size() == 1 && overrides_.layers.empty())
    {
        for (const RadarMapSurface& surface : automatic_->geometry.surfaces)
        {
            if (IsCanceled(token))
            {
                return;
            }

            if (!IsExcluded(layout.analysis.reachability.excluded_surfaces, surface.surface_id) && !surface.sky &&
                surface.normal.z >= 0.0f && surface.vertices.size() >= 3 &&
                (!surface.tiled || support_owners.contains(surface.surface_id)) && !IsRenderRoof(surface, skies))
            {
                meshes_[0].push_back(surface);
            }
        }

        if (ApplyHides(meshes_, overrides_.hides, token))
        {
            RebuildBounds(token);
        }

        return;
    }

    // Collision clearance selects player supports; drawing keeps the source face's contour.
    std::vector<RadarLayerSupport> render_supports =
        RestoreRenderSupports(automatic_->geometry, layout.supports, overrides_, override_ids_, skies, layout.analysis.transitions, token);
    std::map<std::pair<int, int>, std::vector<size_t>> render_index;

    for (size_t i = 0; i < render_supports.size(); i++)
    {
        if (IsCanceled(token))
        {
            return;
        }

        const RadarLayerSupport& support = render_supports[i];
        meshes_[support.layer].push_back(support.surface);

        ncl_math::Aabb2 bounds = Bounds(support.surface.vertices);

        for (int x = Cell(bounds.min.x); x <= Cell(bounds.max.x); x++)
        {
            if (IsCanceled(token))
            {
                return;
            }

            for (int y = Cell(bounds.min.y); y <= Cell(bounds.max.y); y++)
            {
                if (IsCanceled(token))
                {
                    return;
                }

                render_index[{x, y}].push_back(i);
            }
        }
    }

    std::vector<std::set<std::pair<int, std::vector<std::tuple<float, float, float>>>>> detail_keys(layout.layers.size());

    for (const RadarMapSurface& surface : automatic_->geometry.surfaces)
    {
        if (IsCanceled(token))
        {
            return;
        }

        if (IsExcluded(layout.analysis.reachability.excluded_surfaces, surface.surface_id) || surface.sky || surface.tiled ||
            surface.normal.z < 0.0f || support_owners.contains(surface.surface_id) || surface.vertices.size() < 3 ||
            IsRenderRoof(surface, skies))
        {
            continue;
        }

        ncl_math::Aabb2 bounds = Bounds(surface.vertices);

        for (int x = Cell(bounds.min.x); x <= Cell(bounds.max.x); x++)
        {
            if (IsCanceled(token))
            {
                return;
            }

            for (int y = Cell(bounds.min.y); y <= Cell(bounds.max.y); y++)
            {
                if (IsCanceled(token))
                {
                    return;
                }

                auto entry = render_index.find({x, y});

                std::vector<RadarMapVertex> cell = MapGeometry_Clip(
                    surface.vertices, {{x * kCellSize, y * kCellSize, -kUnbounded}, {(x + 1) * kCellSize, (y + 1) * kCellSize, kUnbounded}}
                );

                if (!HasArea(cell))
                {
                    continue;
                }

                std::span<const size_t> local;

                if (entry != render_index.end())
                {
                    local = entry->second;
                }

                for (size_t i : local)
                {
                    const RadarLayerSupport& support = render_supports[i];
                    std::vector<RadarMapVertex> footprint = IntersectPolygons(cell, support.surface.vertices);

                    if (!HasArea(footprint))
                    {
                        continue;
                    }

                    footprint = ClipPlane(footprint, {support.surface.normal, support.surface.vertices[0].pos, 0.0f});

                    if (!HasArea(footprint))
                    {
                        continue;
                    }

                    std::vector<std::vector<RadarMapVertex>> volumes;
                    volumes.push_back(std::move(footprint));

                    for (size_t j : local)
                    {
                        if (i == j)
                        {
                            continue;
                        }

                        std::vector<std::vector<RadarMapVertex>> clipped;

                        for (const std::vector<RadarMapVertex>& volume : volumes)
                        {
                            std::vector<std::vector<RadarMapVertex>> pieces =
                                RemoveAboveSupport(volume, support.surface, render_supports[j].surface);

                            for (std::vector<RadarMapVertex>& piece : pieces)
                            {
                                clipped.push_back(std::move(piece));
                            }
                        }

                        volumes = std::move(clipped);
                    }

                    for (std::vector<RadarMapVertex>& volume : volumes)
                    {
                        ncl_math::Vector3 center{};

                        for (const RadarMapVertex& vertex : volume)
                        {
                            center = center + vertex.pos;
                        }

                        center = center / (float)volume.size();
                        center.z = Height(support.surface, {center.x, center.y});

                        if (!MatchesAssignment(support, center, overrides_, override_ids_))
                        {
                            continue;
                        }

                        RadarMapSurface detail = FragmentSurface(surface, std::move(volume));

                        if (HasArea(detail.vertices))
                        {
                            std::vector<std::tuple<float, float, float>> positions;

                            for (const RadarMapVertex& vertex : detail.vertices)
                            {
                                positions.emplace_back(vertex.pos.x, vertex.pos.y, vertex.pos.z);
                            }

                            std::sort(positions.begin(), positions.end());
                            positions.erase(std::unique(positions.begin(), positions.end()), positions.end());

                            if (detail_keys[support.layer].emplace(detail.surface_id, std::move(positions)).second)
                            {
                                meshes_[support.layer].push_back(std::move(detail));
                            }
                        }
                    }
                }

                std::vector<std::vector<RadarMapVertex>> remaining{cell};

                for (size_t i : local)
                {
                    std::vector<std::vector<RadarMapVertex>> outside;

                    for (const std::vector<RadarMapVertex>& polygon : remaining)
                    {
                        std::vector<std::vector<RadarMapVertex>> pieces =
                            SubtractRenderFootprint(polygon, render_supports[i].surface.vertices);

                        for (std::vector<RadarMapVertex>& piece : pieces)
                        {
                            outside.push_back(std::move(piece));
                        }
                    }

                    remaining = std::move(outside);

                    if (remaining.empty())
                    {
                        break;
                    }
                }

                for (const std::vector<RadarMapVertex>& polygon : remaining)
                {
                    ncl_math::Vector3 center{};

                    for (const RadarMapVertex& vertex : polygon)
                    {
                        center = center + vertex.pos;
                    }

                    center = center / (float)polygon.size();

                    std::vector<DetailOwner> owners;

                    for (const RadarLayerInfo& layer : layout.layers)
                    {
                        RadarLayerMatch match = LocateSupport(center, RadarLayerQuery::Nearest, -1, layer.id, true);

                        if (match.layer < 0)
                        {
                            continue;
                        }

                        DetailOwner owner{};
                        owner.layer = match.layer;
                        owner.automatic_layer = match.layer;
                        float owner_distance = kUnbounded;

                        for (size_t index : support_owners.at(match.surface_id))
                        {
                            const RadarLayerSupport& support = layout.supports[index];

                            if (support.layer != match.layer)
                            {
                                continue;
                            }

                            float current = (ClosestPoint(support.surface, match.support) - match.support).LengthSqr();

                            if (current < owner_distance)
                            {
                                owner_distance = current;
                                owner.automatic_layer = support.automatic_layer;
                                owner.normal = support.surface.normal;
                            }
                        }

                        owner.point = match.support;

                        owners.push_back(std::move(owner));
                    }

                    std::vector<std::vector<RadarMapVertex>> pieces{polygon};

                    for (const DetailOwner& owner : owners)
                    {
                        PolygonPlane plane{owner.normal, owner.point, 0.0f};
                        std::vector<std::vector<RadarMapVertex>> split;

                        for (const std::vector<RadarMapVertex>& piece : pieces)
                        {
                            float minimum = kUnbounded;
                            float maximum = -kUnbounded;

                            for (const RadarMapVertex& vertex : piece)
                            {
                                float distance = plane.normal.Dot(vertex.pos - plane.origin);
                                minimum = std::min(minimum, distance);
                                maximum = std::max(maximum, distance);
                            }

                            if (minimum < 0.0f && maximum > 0.0f)
                            {
                                for (float direction : {-1.0f, 1.0f})
                                {
                                    std::vector<RadarMapVertex> part = ClipPlane(piece, plane, direction);

                                    if (HasArea(part))
                                    {
                                        split.push_back(std::move(part));
                                    }
                                }
                            }
                            else
                            {
                                split.push_back(piece);
                            }
                        }

                        pieces = std::move(split);
                    }

                    for (std::vector<RadarMapVertex>& piece : pieces)
                    {
                        ncl_math::Vector3 point{};

                        for (const RadarMapVertex& vertex : piece)
                        {
                            point = point + vertex.pos;
                        }

                        point = point / (float)piece.size();

                        const DetailOwner* nearest = nullptr;
                        float distance = kUnbounded;

                        for (const DetailOwner& owner : owners)
                        {
                            if (Height(owner.normal, owner.point, {point.x, point.y}) > point.z + kGeometryEpsilon)
                            {
                                continue;
                            }

                            float current = (owner.point - point).LengthSqr();

                            if (current < distance)
                            {
                                nearest = &owner;
                                distance = current;
                            }
                        }

                        if (!nearest)
                        {
                            continue;
                        }

                        std::vector<std::vector<RadarMapVertex>> assigned_pieces = SplitDetailOverrides(piece, *nearest, overrides_);

                        for (std::vector<RadarMapVertex>& assigned_piece : assigned_pieces)
                        {
                            ncl_math::Vector3 feet{};

                            for (const RadarMapVertex& vertex : assigned_piece)
                            {
                                feet = feet + vertex.pos;
                            }

                            feet = feet / (float)assigned_piece.size();
                            feet.z = Height(nearest->normal, nearest->point, {feet.x, feet.y});

                            int assigned = OverrideLayer(feet, overrides_, override_ids_);
                            int layer = assigned >= 0 ? assigned : nearest->automatic_layer;

                            if (assigned == kUnavailableOverride || layer < 0)
                            {
                                continue;
                            }

                            RadarMapSurface detail = FragmentSurface(surface, std::move(assigned_piece));
                            meshes_[layer].push_back(std::move(detail));
                        }
                    }
                }
            }
        }
    }

    if (ApplyHides(meshes_, overrides_.hides, token))
    {
        RebuildBounds(token);
    }
}

bool RadarMapLayers::RebuildBounds(const taskcoro::CancellationToken* token)
{
    bounds_ = ncl_math::Aabb2::Empty();

    for (const std::vector<RadarMapSurface>& mesh : meshes_)
    {
        for (const RadarMapSurface& surface : mesh)
        {
            if (IsCanceled(token))
            {
                return false;
            }

            // Float clipping can give a vertical face a nonzero projected area.
            if (surface.normal.z == 0.0f || SignedArea(surface.vertices) == 0.0f)
            {
                continue;
            }

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                if (IsCanceled(token))
                {
                    return false;
                }

                bounds_.Include({vertex.pos.x, vertex.pos.y});
            }
        }
    }

    if (bounds_.min.x > bounds_.max.x)
    {
        bounds_ = automatic_->geometry.bounds;
    }

    return !IsCanceled(token);
}

void RadarMapLayers::Reset()
{
    automatic_.reset();
    corrected_layout_.reset();
    support_bounds_.clear();

    nearest_ids_.clear();
    nearest_nodes_.clear();
    index_.clear();

    meshes_.clear();
    bounds_ = {};

    overrides_ = {};
    override_ids_.clear();
}

std::span<const RadarLayerInfo> RadarMapLayers::get_layers() const
{
    return get_layout().layers;
}

const ncl_math::Aabb2& RadarMapLayers::get_bounds() const
{
    return bounds_;
}

std::span<const RadarMapSurface> RadarMapLayers::get_surfaces(int layer) const
{
    if (layer < 0 || (size_t)layer >= meshes_.size())
    {
        return {};
    }

    return meshes_[layer];
}

RadarLayerMatch RadarMapLayers::Locate(ncl_math::Vector3 position, RadarLayerQuery query, int preferred, int layer) const
{
    return LocateSupport(position, query, preferred, layer, false);
}

RadarLayerMatch RadarMapLayers::LocateSupport(
    ncl_math::Vector3 position,
    RadarLayerQuery query,
    int preferred,
    int layer,
    bool below_plane
) const
{
    const RadarLayerLayout& layout = get_layout();
    RadarLayerMatch match{};
    float best = kUnbounded;
    int best_source = std::numeric_limits<int>::max();

    auto consider = [&](size_t index) {
        const RadarLayerSupport& support = layout.supports[index];

        if (layer >= 0 && support.layer != layer)
        {
            return;
        }

        if (below_plane && Height(support.surface, {position.x, position.y}) > position.z + kGeometryEpsilon)
        {
            return;
        }

        bool contained = Contains(support.surface.vertices, {position.x, position.y});

        ncl_math::Vector3 point{};

        if (query == RadarLayerQuery::Below)
        {
            if (!contained)
            {
                return;
            }

            point = {position.x, position.y, Height(support.surface, {position.x, position.y})};

            if (point.z > position.z + kGeometryEpsilon)
            {
                return;
            }
        }
        else
        {
            point = ClosestPoint(support.surface, position);
        }

        if (!MatchesAssignment(support, point, overrides_, override_ids_))
        {
            return;
        }

        float distance = (position - point).LengthSqr();
        bool preferred_tie = support.layer == preferred;
        bool best_preferred = match.layer == preferred;

        if (std::tuple{distance, !preferred_tie, support.layer, support.surface.surface_id} <
            std::tuple{best, !best_preferred, match.layer, best_source})
        {
            best = distance;
            best_source = support.surface.surface_id;
            match = {support.layer, point, contained, support.surface.surface_id};
        }
    };

    if (query == RadarLayerQuery::Below)
    {
        auto entry = index_.find({Cell(position.x), Cell(position.y)});

        if (entry != index_.end())
        {
            for (size_t index : entry->second)
            {
                consider(index);
            }
        }
    }
    else
    {
        auto visit = [&](auto&& self, int index) -> void {
            const RadarSupportNode& node = nearest_nodes_[index];

            if (NodeDistanceSquared(node, position, layer) > best)
            {
                return;
            }

            if (node.children[0] < 0)
            {
                for (size_t i = node.first; i < node.first + node.count; i++)
                {
                    consider(nearest_ids_[i]);
                }

                return;
            }

            int first = node.children[0];
            int second = node.children[1];

            if (NodeDistanceSquared(nearest_nodes_[second], position, layer) < NodeDistanceSquared(nearest_nodes_[first], position, layer))
            {
                std::swap(first, second);
            }

            self(self, first);
            self(self, second);
        };

        if (!nearest_nodes_.empty())
        {
            visit(visit, 0);
        }
    }

    return match;
}

const RadarLayerAnalysis& RadarMapLayers::get_analysis() const
{
    return get_layout().analysis;
}

std::vector<int> RadarMapLayers::Intersect(ncl_math::Aabb3 volume) const
{
    std::vector<int> layers;

    Intersect(volume, layers);

    return layers;
}

void RadarMapLayers::Intersect(ncl_math::Aabb3 volume, std::vector<int>& layers) const
{
    const RadarLayerLayout& layout = get_layout();
    layers.clear();
    if (!automatic_)
    {
        return;
    }

    ncl_math::Vector2 minimum{
        std::max(volume.min.x, automatic_->geometry.bounds.min.x), std::max(volume.min.y, automatic_->geometry.bounds.min.y)
    };
    ncl_math::Vector2 maximum{
        std::min(volume.max.x, automatic_->geometry.bounds.max.x), std::min(volume.max.y, automatic_->geometry.bounds.max.y)
    };

    if (minimum.x > maximum.x || minimum.y > maximum.y)
    {
        return;
    }

    std::vector<RadarMapVertex> clipped;
    std::vector<RadarMapVertex> scratch;

    auto consider = [&](size_t index) {
        const RadarLayerSupport& support = layout.supports[index];
        const ncl_math::Aabb3& bounds = support_bounds_[index];

        if (!volume.Overlaps(bounds) || std::find(layers.begin(), layers.end(), support.layer) != layers.end())
        {
            return;
        }

        MapGeometry_Clip(support.surface.vertices, volume, clipped, scratch);

        if (clipped.empty() || std::abs(SignedArea(clipped)) == 0.0f)
        {
            return;
        }

        ncl_math::Vector3 center{};

        for (const RadarMapVertex& vertex : clipped)
        {
            center = center + vertex.pos;
        }

        center = center / (float)clipped.size();

        if (MatchesAssignment(support, center, overrides_, override_ids_))
        {
            layers.push_back(support.layer);
        }
    };

    auto visit = [&](auto&& self, int index) -> void {
        const RadarSupportNode& node = nearest_nodes_[index];

        if (!volume.Overlaps(node.bounds))
        {
            return;
        }

        if (node.children[0] < 0)
        {
            for (size_t i = node.first; i < node.first + node.count; i++)
            {
                consider(nearest_ids_[i]);
            }

            return;
        }

        self(self, node.children[0]);
        self(self, node.children[1]);
    };

    if (!nearest_nodes_.empty())
    {
        visit(visit, 0);
    }

    std::ranges::sort(layers);
}

const RadarLayerLayout& RadarMapLayers::get_layout() const
{
    if (corrected_layout_)
    {
        return *corrected_layout_;
    }
    if (automatic_)
    {
        return automatic_->layout;
    }
    return kEmptyLayout;
}
