#include "MapBspGeometry.h"

#include <cstdlib>
#include <sstream>
#include <utility>

#include <goldsrc/engine/model.h>
#include <goldsrc/event_api.h>
#include <goldsrc/format/bsp.h>
#include <goldsrc/pmove.h>

#include "hud/radar/map/MapReachability.h"

namespace
{
    bool Wait(std::string* reason, std::string message)
    {
        if (reason)
        {
            *reason = std::move(message);
        }

        return false;
    }

    std::string SurfaceReason(const goldsrc::msurface_t& surface, int id, const char* stage)
    {
        std::ostringstream reason;

        reason << "stage=" << stage << " surface=" << id << " flags=" << surface.flags
               << " plane=" << (surface.plane ? "present" : "missing") << " polys=" << (surface.polys ? "present" : "missing")
               << " poly_vertices=" << (surface.polys ? surface.polys->numverts : 0) << " firstedge=" << surface.firstedge
               << " numedges=" << surface.numedges;

        return reason.str();
    }

    bool CopyHull(
        const goldsrc::model_t& world,
        const goldsrc::hull_t& source,
        const ncl_math::Aabb3& bounds,
        RadarCollisionHull& out,
        std::string* reason
    )
    {
        out.offset = ncl_math::Vector3::Ref(source.clip_mins) - bounds.min;

        if (source.firstclipnode < 0)
        {
            out.root = source.firstclipnode;

            return true;
        }

        if (!source.clipnodes || !source.planes || source.firstclipnode > source.lastclipnode || source.lastclipnode >= world.numclipnodes)
        {
            return Wait(
                reason,
                "stage=collision_hull clipnodes=" + std::string(source.clipnodes ? "present" : "missing") +
                    " planes=" + (source.planes ? "present" : "missing") + " first=" + std::to_string(source.firstclipnode) +
                    " last=" + std::to_string(source.lastclipnode) + " world_clipnodes=" + std::to_string(world.numclipnodes)
            );
        }

        std::vector<int> node_ids((size_t)source.lastclipnode + 1, -1);
        std::vector<int> plane_ids(world.numplanes, -1);
        std::vector<int> source_nodes{source.firstclipnode};

        node_ids[source.firstclipnode] = 0;

        for (size_t i = 0; i < source_nodes.size(); i++)
        {
            const goldsrc::dclipnode_t& node = source.clipnodes[source_nodes[i]];

            if (node.planenum < 0 || node.planenum >= world.numplanes)
            {
                return Wait(
                    reason,
                    "stage=collision_plane node=" + std::to_string(source_nodes[i]) + " plane=" + std::to_string(node.planenum) +
                        " world_planes=" + std::to_string(world.numplanes)
                );
            }

            int& plane_id = plane_ids[node.planenum];

            if (plane_id < 0)
            {
                const goldsrc::mplane_t& plane = source.planes[node.planenum];
                plane_id = (int)out.planes.size();
                out.planes.push_back({ncl_math::Vector3::Ref(plane.normal), plane.dist});
            }

            RadarCollisionNode copied{plane_id, {}};

            for (int side = 0; side < 2; side++)
            {
                int child = node.children[side];

                if (child >= 0)
                {
                    if (child < source.firstclipnode || child > source.lastclipnode)
                    {
                        return Wait(
                            reason,
                            "stage=collision_child node=" + std::to_string(source_nodes[i]) + " child=" + std::to_string(child) +
                                " first=" + std::to_string(source.firstclipnode) + " last=" + std::to_string(source.lastclipnode)
                        );
                    }

                    if (node_ids[child] < 0)
                    {
                        node_ids[child] = (int)source_nodes.size();
                        source_nodes.push_back(child);
                    }

                    copied.children[side] = node_ids[child];
                }
                else
                {
                    copied.children[side] = child;
                }
            }

            out.nodes.push_back(copied);
        }

        return true;
    }

    bool CopyUvs(const goldsrc::msurface_t& surface, RadarMapVertex& vertex)
    {
        for (const goldsrc::glpoly_t* poly = surface.polys; poly; poly = poly->next)
        {
            for (int i = 0; i < poly->numverts; i++)
            {
                const float* values = poly->verts[i];

                if (ncl_math::Vector3::Ref(values) == vertex.pos)
                {
                    vertex.uv = {values[3], values[4]};
                    vertex.lightmap_uv = {values[5], values[6]};

                    return true;
                }
            }

            // Both UV sets remain affine when render polygons omit surfedge vertices.
            for (int i = 1; i + 1 < poly->numverts; i++)
            {
                const float* a = poly->verts[0];
                const float* b = poly->verts[i];
                const float* c = poly->verts[i + 1];

                ncl_math::Vector3 start = ncl_math::Vector3::Ref(a);
                ncl_math::Vector3 edge_u = ncl_math::Vector3::Ref(b) - start;
                ncl_math::Vector3 edge_v = ncl_math::Vector3::Ref(c) - start;

                ncl_math::Vector3 normal = edge_u.Cross(edge_v);
                float area_squared = normal.LengthSqr();

                if (area_squared == 0.0f)
                {
                    continue;
                }

                ncl_math::Vector3 delta = vertex.pos - start;
                float u = delta.Cross(edge_v).Dot(normal) / area_squared;
                float v = edge_u.Cross(delta).Dot(normal) / area_squared;

                ncl_math::Vector2 start_uv{a[3], a[4]};
                ncl_math::Vector2 start_lightmap{a[5], a[6]};

                vertex.uv = start_uv + (ncl_math::Vector2{b[3], b[4]} - start_uv) * u + (ncl_math::Vector2{c[3], c[4]} - start_uv) * v;
                vertex.lightmap_uv = start_lightmap + (ncl_math::Vector2{b[5], b[6]} - start_lightmap) * u +
                                     (ncl_math::Vector2{c[5], c[6]} - start_lightmap) * v;

                return true;
            }
        }

        return (surface.flags & (goldsrc::SURF_DRAWSKY | goldsrc::SURF_DRAWTILED)) != 0;
    }

    bool CopySurface(const goldsrc::model_t& world, int surface_id, RadarMapSurface& out, std::string* reason)
    {
        const goldsrc::msurface_t& surface = world.surfaces[surface_id];

        if (!surface.plane || surface.numedges < 3 || surface.firstedge < 0 || surface.firstedge > world.numsurfedges - surface.numedges)
        {
            return Wait(
                reason, SurfaceReason(surface, surface_id, "surface_layout") + " world_surfedges=" + std::to_string(world.numsurfedges)
            );
        }

        out.surface_id = surface_id;

        out.normal = ncl_math::Vector3::Ref(surface.plane->normal);

        if (surface.flags & goldsrc::SURF_PLANEBACK)
        {
            out.normal = -out.normal;
        }

        out.sky = (surface.flags & goldsrc::SURF_DRAWSKY) != 0;
        out.tiled = (surface.flags & goldsrc::SURF_DRAWTILED) != 0;

        out.vertices.reserve(surface.numedges);

        for (int i = 0; i < surface.numedges; i++)
        {
            int surfedge = world.surfedges[surface.firstedge + i];

            if (surfedge <= -world.numedges || surfedge >= world.numedges)
            {
                return Wait(
                    reason,
                    SurfaceReason(surface, surface_id, "surface_edge") + " surfedge=" + std::to_string(surfedge) +
                        " world_edges=" + std::to_string(world.numedges)
                );
            }

            const goldsrc::medge_t& edge = world.edges[std::abs(surfedge)];
            int vertex_id = edge.v[surfedge < 0 ? 1 : 0];

            if (vertex_id >= world.numvertexes)
            {
                return Wait(
                    reason,
                    SurfaceReason(surface, surface_id, "surface_vertex") + " vertex=" + std::to_string(vertex_id) +
                        " world_vertices=" + std::to_string(world.numvertexes)
                );
            }

            RadarMapVertex vertex{ncl_math::Vector3::Ref(world.vertexes[vertex_id].position), {}, {}};

            if (!CopyUvs(surface, vertex))
            {
                return Wait(reason, SurfaceReason(surface, surface_id, "surface_uvs") + " vertex=" + std::to_string(vertex_id));
            }

            out.vertices.push_back(vertex);
        }

        return true;
    }
} // namespace

const goldsrc::hull_t* MapBspGeometry_SelectPlayerHull(
    goldsrc::playermove_t& movement,
    const goldsrc::event_api_s& events,
    goldsrc::model_t& world,
    int player_hull,
    std::string* waiting_reason
)
{
    if (waiting_reason)
    {
        waiting_reason->clear();
    }

    if (!movement.PM_HullForBsp || !events.EV_SetTraceHull)
    {
        Wait(
            waiting_reason,
            !movement.PM_HullForBsp ? "stage=crouch_hull missing=PM_HullForBsp" : "stage=crouch_hull missing=EV_SetTraceHull"
        );

        return nullptr;
    }

    goldsrc::physent_t entity{};
    entity.model = &world;

    float offset[3]{};
    int previous_hull = movement.usehull;

    events.EV_SetTraceHull(player_hull);
    const goldsrc::hull_t* hull = static_cast<const goldsrc::hull_t*>(movement.PM_HullForBsp(&entity, offset));
    events.EV_SetTraceHull(previous_hull);

    if (!hull)
    {
        Wait(waiting_reason, "stage=crouch_hull callback=PM_HullForBsp selected_hull=missing");
    }

    return hull;
}

bool MapBspGeometry_Collect(
    const goldsrc::model_t& world,
    const goldsrc::hull_t& hull,
    const ncl_math::Aabb3& crouch_bounds,
    RadarMapGeometry& out,
    std::string* waiting_reason
)
{
    out = RadarMapGeometry{};

    if (waiting_reason)
    {
        waiting_reason->clear();
    }

    if (world.type != goldsrc::mod_brush || !world.surfaces || !world.vertexes || !world.edges || !world.surfedges ||
        world.firstmodelsurface < 0 || world.nummodelsurfaces <= 0 || world.firstmodelsurface > world.numsurfaces - world.nummodelsurfaces)
    {
        return Wait(
            waiting_reason,
            "stage=world_data type=" + std::to_string(world.type) + " surfaces=" + (world.surfaces ? "present" : "missing") +
                " vertices=" + (world.vertexes ? "present" : "missing") + " edges=" + (world.edges ? "present" : "missing") +
                " surfedges=" + (world.surfedges ? "present" : "missing") + " first_surface=" + std::to_string(world.firstmodelsurface) +
                " model_surfaces=" + std::to_string(world.nummodelsurfaces) + " world_surfaces=" + std::to_string(world.numsurfaces)
        );
    }

    RadarMapGeometry geometry{};
    geometry.bounds = {{world.mins[0], world.mins[1]}, {world.maxs[0], world.maxs[1]}};
    geometry.crouch_bounds = crouch_bounds;

    if (world.entities)
    {
        MapReachability_ReadEntryPoints(world.entities, crouch_bounds.min.z, geometry.entry_points);
    }

    RadarCollisionHull collision{};

    if (!CopyHull(world, hull, crouch_bounds, collision, waiting_reason))
    {
        return false;
    }

    geometry.collision_hulls.push_back(std::move(collision));

    geometry.surfaces.reserve(world.nummodelsurfaces);

    for (int i = world.firstmodelsurface; i < world.firstmodelsurface + world.nummodelsurfaces; i++)
    {
        RadarMapSurface surface{};

        if (!CopySurface(world, i, surface, waiting_reason))
        {
            return false;
        }

        geometry.surfaces.push_back(std::move(surface));
    }

    out = std::move(geometry);

    return true;
}
