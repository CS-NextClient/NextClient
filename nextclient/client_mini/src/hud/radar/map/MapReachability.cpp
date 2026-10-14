#include "MapReachability.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <span>
#include <unordered_set>
#include <utility>

#include <goldsrc/format/bsp.h>
#include <ncl_math/clip.h>
#include <taskcoro/CancellationToken.h>

namespace
{
    constexpr double kPortalTolerance = 0.001;
    constexpr int kBoundsCorners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    // Double precision keeps successive BSP cuts from collapsing narrow collision portals.
    using CollisionPoint_t = std::array<double, 3>;

    struct EntryEntity
    {
        std::string_view classname;
        std::string_view origin;
        std::string_view targetname;
    };

    struct CollisionFace
    {
        int plane = -1;
        int side{};
        std::vector<CollisionPoint_t> vertices;
    };

    struct CollisionBounds
    {
        CollisionPoint_t min{};
        CollisionPoint_t max{};
    };

    struct CollisionRegion
    {
        std::vector<CollisionFace> faces;
        CollisionBounds bounds;
        std::vector<size_t> neighbors;
        bool connected{};
    };

    struct CollisionTreeNode
    {
        int source = -1;
        int children[2] = {-1, -1};
        int region = -1;
    };

    struct CollisionPortal
    {
        size_t region{};
        std::vector<CollisionPoint_t> vertices;
        CollisionBounds bounds;
    };

    bool IsCanceled(const taskcoro::CancellationToken* token)
    {
        return token && token->IsCanceled();
    }

    CollisionPoint_t Point(ncl_math::Vector3 point)
    {
        return {point.x, point.y, point.z};
    }

    double Dot(const CollisionPoint_t& a, const CollisionPoint_t& b)
    {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    CollisionPoint_t Subtract(const CollisionPoint_t& a, const CollisionPoint_t& b)
    {
        return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    }

    CollisionPoint_t Cross(const CollisionPoint_t& a, const CollisionPoint_t& b)
    {
        return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    }

    CollisionBounds EmptyBounds()
    {
        double limit = std::numeric_limits<double>::infinity();
        return {{limit, limit, limit}, {-limit, -limit, -limit}};
    }

    void Include(CollisionBounds& bounds, const CollisionPoint_t& point)
    {
        for (int axis = 0; axis < 3; axis++)
        {
            bounds.min[axis] = std::min(bounds.min[axis], point[axis]);
            bounds.max[axis] = std::max(bounds.max[axis], point[axis]);
        }
    }

    void Clip(
        std::span<const CollisionPoint_t> polygon,
        const CollisionPoint_t& normal,
        double distance,
        double direction,
        std::vector<CollisionPoint_t>& clipped
    )
    {
        clipped.clear();
        ncl_math::ClipPolygonHalfSpace<CollisionPoint_t>(
            polygon,
            [&](const CollisionPoint_t& point) { return (Dot(point, normal) - distance) * direction; },
            [](const CollisionPoint_t& previous, const CollisionPoint_t& point, double fraction) {
                return CollisionPoint_t{
                    previous[0] + (point[0] - previous[0]) * fraction,
                    previous[1] + (point[1] - previous[1]) * fraction,
                    previous[2] + (point[2] - previous[2]) * fraction,
                };
            },
            [&](const CollisionPoint_t& point) { clipped.push_back(point); }
        );
    }

    std::vector<CollisionPoint_t> Clip(
        std::span<const CollisionPoint_t> polygon,
        const CollisionPoint_t& normal,
        double distance,
        double direction
    )
    {
        std::vector<CollisionPoint_t> clipped;
        Clip(polygon, normal, distance, direction, clipped);
        return clipped;
    }

    double Area(std::span<const CollisionPoint_t> polygon)
    {
        double area = 0.0;
        for (size_t i = 0; i < polygon.size(); i++)
        {
            const CollisionPoint_t& a = polygon[i];
            const CollisionPoint_t& b = polygon[(i + 1) % polygon.size()];
            area += a[0] * b[1] - a[1] * b[0];
        }
        return area * 0.5;
    }

    bool Touches(const CollisionPortal& a, const CollisionPortal& b)
    {
        if (a.bounds.min[0] > b.bounds.max[0] + kPortalTolerance || b.bounds.min[0] > a.bounds.max[0] + kPortalTolerance ||
            a.bounds.min[1] > b.bounds.max[1] + kPortalTolerance || b.bounds.min[1] > a.bounds.max[1] + kPortalTolerance)
        {
            return false;
        }

        std::vector<CollisionPoint_t> polygon = a.vertices;
        std::vector<CollisionPoint_t> scratch;
        double direction = Area(b.vertices) >= 0.0 ? 1.0 : -1.0;
        for (size_t i = 0; i < b.vertices.size(); i++)
        {
            const CollisionPoint_t& first = b.vertices[i];
            const CollisionPoint_t& second = b.vertices[(i + 1) % b.vertices.size()];
            CollisionPoint_t normal{-(second[1] - first[1]) * direction, (second[0] - first[0]) * direction, 0.0};
            double distance = Dot(normal, first) - kPortalTolerance * std::hypot(normal[0], normal[1]);
            Clip(polygon, normal, distance, 1.0, scratch);
            polygon.swap(scratch);
            if (polygon.empty())
            {
                return false;
            }
        }
        return true;
    }

    std::vector<CollisionFace> BoundsFaces(const RadarMapGeometry& geometry, const RadarCollisionHull& hull)
    {
        CollisionBounds bounds = EmptyBounds();
        for (const RadarMapSurface& surface : geometry.surfaces)
        {
            for (const RadarMapVertex& vertex : surface.vertices)
            {
                Include(bounds, Point(vertex.pos - hull.offset));
            }
        }
        for (ncl_math::Vector3 entry : geometry.entry_points)
        {
            Include(bounds, Point(entry - hull.offset));
        }
        double margin = 0.0;
        for (int axis = 0; axis < 3; axis++)
        {
            margin += std::max(std::abs(geometry.crouch_bounds.min[axis]), std::abs(geometry.crouch_bounds.max[axis]));
        }
        for (int axis = 0; axis < 3; axis++)
        {
            bounds.min[axis] -= margin + kPortalTolerance;
            bounds.max[axis] += margin + kPortalTolerance;
        }
        std::vector<CollisionFace> faces;
        for (int axis = 0; axis < 3; axis++)
        {
            int first = (axis + 1) % 3;
            int second = (axis + 2) % 3;
            for (int side = 0; side < 2; side++)
            {
                CollisionFace face;
                for (const int* corner : kBoundsCorners)
                {
                    CollisionPoint_t point{};
                    point[axis] = side == 0 ? bounds.min[axis] : bounds.max[axis];
                    point[first] = corner[0] == 0 ? bounds.min[first] : bounds.max[first];
                    point[second] = corner[1] == 0 ? bounds.min[second] : bounds.max[second];
                    face.vertices.push_back(point);
                }
                faces.push_back(std::move(face));
            }
        }
        return faces;
    }

    class CollisionRegions
    {
    private:
        const RadarCollisionHull& hull_;
        const taskcoro::CancellationToken* token_{};
        std::vector<CollisionTreeNode> nodes_;
        std::vector<CollisionRegion> regions_;
        std::map<int, std::array<std::vector<CollisionPortal>, 2>> portals_;
        int root_ = -1;
        size_t valid_entries_{};

    public:
        CollisionRegions(const RadarCollisionHull& hull, const taskcoro::CancellationToken* token) :
            hull_(hull),
            token_(token)
        {}

        void Build(const RadarMapGeometry& geometry)
        {
            root_ = Divide(hull_.root, BoundsFaces(geometry, hull_));

            if (geometry.entry_points.empty())
            {
                return;
            }

            for (const auto& entry : portals_)
            {
                const std::array<std::vector<CollisionPortal>, 2>& portals = entry.second;
                for (const CollisionPortal& first : portals[0])
                {
                    for (const CollisionPortal& second : portals[1])
                    {
                        if (IsCanceled(token_))
                        {
                            return;
                        }
                        if (first.region != second.region && Touches(first, second))
                        {
                            regions_[first.region].neighbors.push_back(second.region);
                            regions_[second.region].neighbors.push_back(first.region);
                        }
                    }
                }
            }

            std::vector<size_t> pending;
            for (ncl_math::Vector3 entry : geometry.entry_points)
            {
                int region = Locate(Point(entry - hull_.offset));
                if (region >= 0)
                {
                    valid_entries_++;
                    if (!regions_[region].connected)
                    {
                        regions_[region].connected = true;
                        pending.push_back((size_t)region);
                    }
                }
            }
            for (size_t i = 0; i < pending.size(); i++)
            {
                if (IsCanceled(token_))
                {
                    return;
                }
                for (size_t neighbor : regions_[pending[i]].neighbors)
                {
                    if (!regions_[neighbor].connected)
                    {
                        regions_[neighbor].connected = true;
                        pending.push_back(neighbor);
                    }
                }
            }
        }

        bool HasContact(
            const RadarMapSurface& surface,
            const ncl_math::Aabb3& body,
            bool connected_only
        ) const
        {
            float x = surface.normal.x > 0.0f ? body.min.x : body.max.x;
            float y = surface.normal.y > 0.0f ? body.min.y : body.max.y;
            double lift = -body.min.z - (surface.normal.x * x + surface.normal.y * y) / surface.normal.z + kPortalTolerance;
            double pad_x = std::max(std::abs(body.min.x), std::abs(body.max.x));
            double pad_y = std::max(std::abs(body.min.y), std::abs(body.max.y));
            std::vector<CollisionPoint_t> polygon;
            CollisionBounds bounds = EmptyBounds();
            for (const RadarMapVertex& vertex : surface.vertices)
            {
                CollisionPoint_t point = Point(vertex.pos - hull_.offset);
                point[2] += lift;
                Include(bounds, point);
                polygon.push_back(point);
            }
            // Render floors can lie below collision floors; contact uncertainty extends upward only.
            bounds.max[2] += body.max.z;
            std::vector<CollisionPoint_t> contact;
            std::vector<CollisionPoint_t> scratch;
            for (const CollisionRegion& region : regions_)
            {
                if (IsCanceled(token_))
                {
                    return true;
                }
                if ((connected_only && !region.connected) ||
                    bounds.min[0] > region.bounds.max[0] + pad_x || bounds.max[0] < region.bounds.min[0] - pad_x ||
                    bounds.min[1] > region.bounds.max[1] + pad_y || bounds.max[1] < region.bounds.min[1] - pad_y ||
                    bounds.min[2] > region.bounds.max[2] + kPortalTolerance || bounds.max[2] < region.bounds.min[2] - kPortalTolerance)
                {
                    continue;
                }
                contact.assign(polygon.begin(), polygon.end());
                for (const CollisionFace& face : region.faces)
                {
                    if (face.plane < 0)
                    {
                        continue;
                    }
                    const RadarCollisionPlane& plane = hull_.planes[face.plane];
                    double direction = face.side == 0 ? 1.0 : -1.0;
                    double padding = std::abs(plane.normal.x) * pad_x + std::abs(plane.normal.y) * pad_y +
                                     std::max(0.0, direction * plane.normal.z) * body.max.z + kPortalTolerance;
                    Clip(contact, Point(plane.normal), plane.dist - direction * padding, direction, scratch);
                    contact.swap(scratch);
                    if (contact.empty())
                    {
                        break;
                    }
                }
                if (!contact.empty())
                {
                    return true;
                }
            }
            return false;
        }

        size_t get_region_count() const
        {
            return regions_.size();
        }
        size_t get_valid_entry_count() const
        {
            return valid_entries_;
        }
        size_t get_connected_region_count() const
        {
            return (size_t)std::count_if(regions_.begin(), regions_.end(), [](const CollisionRegion& region) { return region.connected; });
        }

    private:
        int Locate(const CollisionPoint_t& point) const
        {
            int node = root_;
            while (node >= 0 && nodes_[node].source >= 0)
            {
                const RadarCollisionNode& source = hull_.nodes[nodes_[node].source];
                const RadarCollisionPlane& plane = hull_.planes[source.plane];
                int side = Dot(point, Point(plane.normal)) < plane.dist ? 1 : 0;
                node = nodes_[node].children[side];
            }
            return node >= 0 ? nodes_[node].region : -1;
        }

        void AddRegion(int node, std::vector<CollisionFace> faces)
        {
            size_t id = regions_.size();
            CollisionRegion region;
            region.bounds = EmptyBounds();
            for (const CollisionFace& face : faces)
            {
                for (const CollisionPoint_t& point : face.vertices)
                {
                    Include(region.bounds, point);
                }
                if (face.plane < 0)
                {
                    continue;
                }
                const ncl_math::Vector3& normal = hull_.planes[face.plane].normal;
                int axis = 0;
                for (int a = 1; a < 3; a++)
                {
                    if (std::abs(normal[a]) > std::abs(normal[axis]))
                    {
                        axis = a;
                    }
                }
                CollisionPortal portal;
                portal.region = id;
                portal.bounds = EmptyBounds();
                for (const CollisionPoint_t& point : face.vertices)
                {
                    CollisionPoint_t projected{point[(axis + 1) % 3], point[(axis + 2) % 3], 0.0};
                    portal.vertices.push_back(projected);
                    Include(portal.bounds, projected);
                }
                if (Area(portal.vertices) != 0.0)
                {
                    portals_[face.plane][face.side].push_back(std::move(portal));
                }
            }
            region.faces = std::move(faces);
            regions_.push_back(std::move(region));
            nodes_[node].region = (int)id;
        }

        int Divide(int source, std::vector<CollisionFace> faces)
        {
            if (IsCanceled(token_) || source == goldsrc::CONTENTS_SOLID || source == goldsrc::CONTENTS_SKY)
            {
                return -1;
            }
            int id = (int)nodes_.size();
            nodes_.emplace_back();
            if (source < 0)
            {
                AddRegion(id, std::move(faces));
                return id;
            }
            nodes_[id].source = source;
            const RadarCollisionNode& node = hull_.nodes[source];
            const RadarCollisionPlane& plane = hull_.planes[node.plane];
            CollisionPoint_t normal = Point(plane.normal);
            double minimum = std::numeric_limits<double>::infinity();
            double maximum = -minimum;
            for (const CollisionFace& face : faces)
            {
                for (const CollisionPoint_t& point : face.vertices)
                {
                    double distance = Dot(point, normal) - plane.dist;
                    minimum = std::min(minimum, distance);
                    maximum = std::max(maximum, distance);
                }
            }
            if (minimum >= 0.0 || maximum <= 0.0)
            {
                int side = minimum >= 0.0 ? 0 : 1;
                int child = Divide(node.children[side], std::move(faces));
                nodes_[id].children[side] = child;
                return id;
            }

            std::array<std::vector<CollisionFace>, 2> parts;
            std::vector<CollisionPoint_t> cap;
            for (const CollisionFace& face : faces)
            {
                for (int side = 0; side < 2; side++)
                {
                    CollisionFace fragment{face.plane, face.side, Clip(face.vertices, normal, plane.dist, side == 0 ? 1.0 : -1.0)};
                    if (fragment.vertices.size() < 3)
                    {
                        continue;
                    }
                    for (const CollisionPoint_t& point : fragment.vertices)
                    {
                        if (std::abs(Dot(point, normal) - plane.dist) <= kPortalTolerance &&
                            std::none_of(cap.begin(), cap.end(), [&](const CollisionPoint_t& other) {
                                CollisionPoint_t difference = Subtract(point, other);
                                return Dot(difference, difference) <= kPortalTolerance * kPortalTolerance;
                            }))
                        {
                            cap.push_back(point);
                        }
                    }
                    parts[side].push_back(std::move(fragment));
                }
            }
            if (cap.size() >= 3)
            {
                CollisionPoint_t center{};
                for (const CollisionPoint_t& point : cap)
                {
                    for (int axis = 0; axis < 3; axis++)
                    {
                        center[axis] += point[axis] / (double)cap.size();
                    }
                }
                int axis = 0;
                for (int a = 1; a < 3; a++)
                {
                    if (std::abs(normal[a]) < std::abs(normal[axis]))
                    {
                        axis = a;
                    }
                }
                CollisionPoint_t unit{};
                unit[axis] = 1.0;
                CollisionPoint_t right = Cross(normal, unit);
                CollisionPoint_t up = Cross(normal, right);
                std::sort(cap.begin(), cap.end(), [&](const CollisionPoint_t& a, const CollisionPoint_t& b) {
                    return std::atan2(Dot(Subtract(a, center), up), Dot(Subtract(a, center), right)) <
                           std::atan2(Dot(Subtract(b, center), up), Dot(Subtract(b, center), right));
                });
                for (int side = 0; side < 2; side++)
                {
                    parts[side].push_back({node.plane, side, cap});
                }
            }
            for (int side = 0; side < 2; side++)
            {
                if (!parts[side].empty())
                {
                    int child = Divide(node.children[side], std::move(parts[side]));
                    nodes_[id].children[side] = child;
                }
            }
            return id;
        }
    };

    bool ReadToken(std::string_view input, size_t& cursor, std::string_view& token, bool& malformed)
    {
        while (cursor < input.size())
        {
            char value = input[cursor];
            if (value == '\0' || value == ' ' || value == '\t' || value == '\r' || value == '\n')
            {
                cursor++;
            }
            else if (input.substr(cursor, 2) == "//")
            {
                size_t end = input.find('\n', cursor);
                cursor = end == std::string_view::npos ? input.size() : end + 1;
            }
            else
            {
                break;
            }
        }
        if (cursor == input.size())
        {
            return false;
        }
        size_t start = cursor++;
        if (input[start] == '"')
        {
            size_t end = input.find('"', cursor);
            if (end == std::string_view::npos)
            {
                malformed = true;
                cursor = input.size();
                return false;
            }
            token = input.substr(cursor, end - cursor);
            cursor = end + 1;
        }
        else if (input[start] == '{' || input[start] == '}')
        {
            token = input.substr(start, 1);
        }
        else
        {
            while (cursor < input.size() && input[cursor] > ' ' && input[cursor] != '{' && input[cursor] != '}')
            {
                cursor++;
            }
            token = input.substr(start, cursor - start);
        }
        return true;
    }

    bool ReadOrigin(std::string_view text, ncl_math::Vector3& origin)
    {
        for (int axis = 0; axis < 3; axis++)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            {
                text.remove_prefix(1);
            }
            if (!text.empty() && text.front() == '+')
            {
                text.remove_prefix(1);
            }
            if (text.empty())
            {
                return false;
            }
            const char* begin = text.data();
            const char* end = begin + text.size();
            std::from_chars_result parsed = std::from_chars(begin, end, origin[axis]);
            if (parsed.ec != std::errc{} || !std::isfinite(origin[axis]))
            {
                return false;
            }
            text.remove_prefix((size_t)(parsed.ptr - begin));
        }
        return text.find_first_not_of(" \t\r\n") == std::string_view::npos;
    }
} // namespace

bool MapReachability_ReadEntryPoints(std::string_view entities, float crouch_min_z, std::vector<ncl_math::Vector3>& out)
{
    out.clear();
    std::vector<EntryEntity> entries;
    std::unordered_set<std::string_view> teleport_targets;
    size_t cursor = 0;
    std::string_view token;
    bool malformed = false;
    while (ReadToken(entities, cursor, token, malformed))
    {
        if (token != "{")
        {
            return false;
        }
        EntryEntity entity;
        std::string_view target;
        while (ReadToken(entities, cursor, token, malformed) && token != "}")
        {
            std::string_view key = token;
            if (!ReadToken(entities, cursor, token, malformed) || token == "{" || token == "}")
            {
                return false;
            }
            if (key == "classname")
            {
                entity.classname = token;
            }
            else if (key == "origin")
            {
                entity.origin = token;
            }
            else if (key == "targetname")
            {
                entity.targetname = token;
            }
            else if (key == "target")
            {
                target = token;
            }
        }
        if (token != "}")
        {
            return false;
        }
        if (entity.classname == "trigger_teleport" && !target.empty())
        {
            teleport_targets.insert(target);
        }
        entries.push_back(entity);
    }
    if (malformed)
    {
        return false;
    }
    for (const EntryEntity& entity : entries)
    {
        bool teleport = entity.classname == "info_teleport_destination" || teleport_targets.contains(entity.targetname);
        if (teleport || entity.classname == "info_player_start" || entity.classname == "info_player_deathmatch" ||
            entity.classname == "info_player_coop")
        {
            ncl_math::Vector3 origin{};
            if (!ReadOrigin(entity.origin, origin))
            {
                out.clear();
                return false;
            }
            if (teleport)
            {
                // Teleport destinations specify feet; the collision hull tests the player's center.
                origin.z += 1.0f - crouch_min_z;
            }
            out.push_back(origin);
        }
    }
    return true;
}

RadarMapReachability MapReachability_Analyze(const RadarMapGeometry& geometry, const taskcoro::CancellationToken* token)
{
    RadarMapReachability result;

    if (geometry.collision_hulls.size() != 1)
    {
        result.reason = "world_collision_hull_unavailable";
        return result;
    }

    bool has_entries = !geometry.entry_points.empty();
    CollisionRegions regions(geometry.collision_hulls[0], token);
    regions.Build(geometry);

    if (IsCanceled(token))
    {
        result.reason = "canceled";
        return result;
    }
    result.regions = regions.get_region_count();
    result.connected_regions = regions.get_connected_region_count();
    result.valid_entry_points = regions.get_valid_entry_count();
    if (has_entries && result.valid_entry_points == 0)
    {
        result.reason = "entry_points_blocked";
        return result;
    }
    for (const RadarMapSurface& surface : geometry.surfaces)
    {
        if (IsCanceled(token))
        {
            result.excluded_surfaces.clear();
            result.reason = "canceled";
            return result;
        }
        if (!surface.sky && surface.normal.z >= kMapSupportNormalZ && surface.vertices.size() >= 3 &&
            !regions.HasContact(surface, geometry.crouch_bounds, has_entries))
        {
            result.excluded_surfaces.push_back(surface.surface_id);
        }
    }
    if (IsCanceled(token))
    {
        result.excluded_surfaces.clear();
        result.reason = "canceled";
        return result;
    }
    std::sort(result.excluded_surfaces.begin(), result.excluded_surfaces.end());
    result.excluded_surfaces.erase(
        std::unique(result.excluded_surfaces.begin(), result.excluded_surfaces.end()), result.excluded_surfaces.end()
    );
    result.available = true;
    return result;
}
