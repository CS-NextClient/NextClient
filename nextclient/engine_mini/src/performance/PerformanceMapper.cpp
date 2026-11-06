#include "performance/PerformanceMapper.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/entity.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/view.h>

namespace performance
{
    namespace
    {
        constexpr float kTraceDistance = 65536.0f;
        constexpr float kPlaneEpsilon = 0.01f;
        constexpr uint32_t kFacesPerNode = 8;

        bool ReadToken(const char*& text, std::string& token, bool& quoted)
        {
            token.clear();
            quoted = false;
            for (;;)
            {
                while (*text && static_cast<unsigned char>(*text) <= ' ')
                {
                    ++text;
                }
                if (text[0] != '/' || text[1] != '/')
                {
                    break;
                }
                while (*text && *text != '\n')
                {
                    ++text;
                }
            }
            if (!*text)
            {
                return false;
            }
            if (*text == '{' || *text == '}')
            {
                token += *text++;
                return true;
            }
            if (*text == '"')
            {
                quoted = true;
                ++text;
                while (*text && *text != '"')
                {
                    token += *text++;
                }
                if (!*text)
                {
                    return false;
                }
                ++text;
                return true;
            }
            while (*text && static_cast<unsigned char>(*text) > ' ' && *text != '{' && *text != '}')
            {
                token += *text++;
            }
            return true;
        }

        bool RayBox(ncl_math::Vector3 origin, ncl_math::Vector3 direction, const ncl_math::Aabb3& bounds, float& distance)
        {
            float enter = 0;
            float leave = distance;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (std::abs(direction[axis]) < 1.0e-8f)
                {
                    if (origin[axis] < bounds.min[axis] - kPlaneEpsilon || origin[axis] > bounds.max[axis] + kPlaneEpsilon)
                    {
                        return false;
                    }
                    continue;
                }
                float first = (bounds.min[axis] - origin[axis]) / direction[axis];
                float last = (bounds.max[axis] - origin[axis]) / direction[axis];
                if (first > last)
                {
                    std::swap(first, last);
                }
                enter = std::max(enter, first);
                leave = std::min(leave, last);
                if (enter > leave + kPlaneEpsilon)
                {
                    return false;
                }
            }
            distance = enter;
            return true;
        }

        ncl_math::Vector3 SurfaceVertex(const goldsrc::model_t& model, const goldsrc::msurface_t& surface, int edge)
        {
            int surfedge = model.surfedges[surface.firstedge + edge];
            const goldsrc::medge_t& vertices = model.edges[std::abs(surfedge)];
            return ncl_math::Vector3::Ref(model.vertexes[vertices.v[surfedge < 0 ? 1 : 0]].position);
        }

        bool RaySurface(
            const goldsrc::model_t& model,
            const goldsrc::msurface_t& surface,
            ncl_math::Vector3 origin,
            ncl_math::Vector3 direction,
            float& distance
        )
        {
            ncl_math::Vector3 normal = ncl_math::Vector3::Ref(surface.plane->normal);
            float denominator = normal.Dot(direction);
            if (std::abs(denominator) < 1.0e-8f)
            {
                return false;
            }
            float hit = (surface.plane->dist - normal.Dot(origin)) / denominator;
            if (hit < kPlaneEpsilon || hit >= distance)
            {
                return false;
            }
            ncl_math::Vector3 point = origin + direction * hit;
            bool positive = false;
            bool negative = false;
            for (int edge = 0; edge < surface.numedges; ++edge)
            {
                ncl_math::Vector3 start = SurfaceVertex(model, surface, edge);
                ncl_math::Vector3 end = SurfaceVertex(model, surface, (edge + 1) % surface.numedges);
                float side = (end - start).Cross(point - start).Dot(normal);
                positive |= side > kPlaneEpsilon;
                negative |= side < -kPlaneEpsilon;
                if (positive && negative)
                {
                    return false;
                }
            }
            distance = hit;
            return true;
        }

        ncl_math::Aabb3 ModelBounds(const goldsrc::cl_entity_t& entity)
        {
            const goldsrc::model_t& model = *entity.model;
            ncl_math::Aabb3 bounds{ncl_math::Vector3::Ref(model.mins), ncl_math::Vector3::Ref(model.maxs)};
            if (model.type == goldsrc::mod_studio && model.cache.data)
            {
                const goldsrc::studiohdr_t& header = *static_cast<const goldsrc::studiohdr_t*>(model.cache.data);
                bounds = {ncl_math::Vector3::Ref(header.bbmin), ncl_math::Vector3::Ref(header.bbmax)};
                if (bounds.min == bounds.max)
                {
                    bounds = {ncl_math::Vector3::Ref(header.min), ncl_math::Vector3::Ref(header.max)};
                }
                int sequence = entity.curstate.sequence;
                if (sequence >= 0 && sequence < header.numseq)
                {
                    // Studio model caches retain sequence descriptors before texture data.
                    const goldsrc::mstudioseqdesc_t* sequences =
                        reinterpret_cast<const goldsrc::mstudioseqdesc_t*>(static_cast<const uint8_t*>(model.cache.data) + header.seqindex);
                    const goldsrc::mstudioseqdesc_t& description = sequences[sequence];
                    bounds = {ncl_math::Vector3::Ref(description.bbmin), ncl_math::Vector3::Ref(description.bbmax)};
                }
            }
            if (model.type == goldsrc::mod_sprite && entity.curstate.scale > 0)
            {
                bounds.min = bounds.min * entity.curstate.scale;
                bounds.max = bounds.max * entity.curstate.scale;
            }
            return bounds;
        }

        std::string Coordinates(ncl_math::Vector3 point)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "%.1f %.1f %.1f", point.x, point.y, point.z);
            return text;
        }
    }

    void PerformanceMapper::SetLevel(int level, const goldsrc::client_static_t* client, const goldsrc::server_t* server)
    {
        level = std::clamp(level, 0, 2);
        if (level == 2 && !(client && client->state == goldsrc::ca_active && !client->demoplayback &&
                            client->netchan.remote_address.type == goldsrc::NA_LOOPBACK && server && server->active))
        {
            level = 1;
        }
        if (snapshot_.level != level)
        {
            snapshot_.level = level;
            snapshot_.enabled = level != 0;
            Reset();
        }
    }

    bool PerformanceMapper::is_enabled() const
    {
        return snapshot_.enabled;
    }

    bool PerformanceMapper::HandleKey(int key, bool down, bool gameplay)
    {
        if (key != 'e' && key != 'E')
        {
            return false;
        }
        bool pressed = down && !dump_key_down_;
        dump_key_down_ = down;
        return pressed && gameplay && snapshot_.enabled && snapshot_.map_loaded && snapshot_.view_measured;
    }

    void PerformanceMapper::ResetInput()
    {
        dump_key_down_ = false;
    }

    void PerformanceMapper::Reset()
    {
        int level = snapshot_.level;
        uint64_t revision = snapshot_.revision + 1;
        snapshot_ = {};
        snapshot_.revision = revision;
        snapshot_.level = level;
        snapshot_.enabled = level != 0;
        map_entities_.clear();
        world_faces_.clear();
        world_nodes_.clear();
        world_ = nullptr;
        shader_triangles_ = 0;
        map_generation_ = 0;
        view_area_ = 0;
        ResetInput();
    }

    void PerformanceMapper::BeginFrame()
    {
        view_area_ = 0;
        snapshot_.view_measured = false;
        snapshot_.target_hit = false;
        snapshot_.target_name.clear();
        snapshot_.target_origin = {};
        snapshot_.properties.clear();
        snapshot_.outline.clear();
        ++snapshot_.revision;
    }

    void PerformanceMapper::BeginView(int& wpoly, int& epoly)
    {
        shader_triangles_ = 0;
        wpoly = 0;
        epoly = 0;
    }

    void PerformanceMapper::AddShaderTriangles(uint64_t triangles)
    {
        if (snapshot_.enabled)
        {
            shader_triangles_ += triangles;
        }
    }

    void PerformanceMapper::UpdateWorld(const goldsrc::client_state_t* client, uint32_t map_generation)
    {
        if (!snapshot_.enabled)
        {
            return;
        }

        const goldsrc::model_t* world = client ? client->worldmodel : nullptr;

        if (!world || map_generation_ != map_generation || world_ != world)
        {
            Reset();

            if (!world)
            {
                return;
            }

            world_ = world;
            map_generation_ = map_generation;
            snapshot_.map_loaded = true;
            ParseEntities(world->entities);
            BuildWorldFaces();
        }

        snapshot_.position = ncl_math::Vector3::Ref(client->simorg);
    }

    void PerformanceMapper::ParseEntities(const char* text)
    {
        if (!text)
        {
            return;
        }
        std::vector<MapEntity> entities;
        uint32_t brushes = 0;
        std::string token;
        bool quoted = false;
        while (ReadToken(text, token, quoted))
        {
            if (quoted || token != "{")
            {
                return;
            }
            MapEntity entity;
            bool closed = false;
            while (ReadToken(text, token, quoted))
            {
                if (!quoted && token == "}")
                {
                    closed = true;
                    break;
                }
                std::string value;
                if ((!quoted && token == "{") || !ReadToken(text, value, quoted) || (!quoted && (value == "}" || value == "{")))
                {
                    return;
                }
                if (token == "model")
                {
                    entity.model = value;
                }
                else if (token == "classname")
                {
                    entity.classname = value;
                }
                else if (token == "targetname")
                {
                    entity.targetname = value;
                }
                entity.properties.push_back({token, std::move(value)});
            }
            if (!closed)
            {
                return;
            }
            if (entity.classname != "worldspawn" && entity.model.starts_with('*'))
            {
                ++brushes;
            }
            entities.push_back(std::move(entity));
        }
        map_entities_ = std::move(entities);
        snapshot_.brush_entities = brushes;
        snapshot_.map_entities_available = !map_entities_.empty();
    }

    void PerformanceMapper::BuildWorldFaces()
    {
        for (int index = world_->firstmodelsurface; index < world_->firstmodelsurface + world_->nummodelsurfaces; ++index)
        {
            const goldsrc::msurface_t& surface = world_->surfaces[index];
            if (surface.numedges < 3)
            {
                continue;
            }
            ncl_math::Aabb3 bounds = ncl_math::Aabb3::Empty();
            for (int edge = 0; edge < surface.numedges; ++edge)
            {
                bounds.Include(SurfaceVertex(*world_, surface, edge));
            }
            world_faces_.push_back({index, bounds});
        }
        if (!world_faces_.empty())
        {
            BuildWorldNode(0, uint32_t(world_faces_.size()));
        }
    }

    uint32_t PerformanceMapper::BuildWorldNode(uint32_t first, uint32_t count)
    {
        uint32_t node = uint32_t(world_nodes_.size());
        world_nodes_.emplace_back();
        ncl_math::Aabb3 bounds = ncl_math::Aabb3::Empty();
        for (uint32_t index = first; index < first + count; ++index)
        {
            bounds.Include(world_faces_[index].bounds);
        }
        world_nodes_[node].bounds = bounds;
        world_nodes_[node].first = first;
        world_nodes_[node].count = count;
        if (count > kFacesPerNode)
        {
            ncl_math::Vector3 size = bounds.Size();
            int axis = size.y > size.x ? 1 : 0;
            axis = size.z > size[axis] ? 2 : axis;
            uint32_t half = count / 2;
            std::nth_element(
                world_faces_.begin() + first,
                world_faces_.begin() + first + half,
                world_faces_.begin() + first + count,
                [axis](const WorldFace& left, const WorldFace& right) { return left.bounds.Center()[axis] < right.bounds.Center()[axis]; }
            );
            uint32_t left = BuildWorldNode(first, half);
            uint32_t right = BuildWorldNode(first + half, count - half);
            world_nodes_[node].children[0] = left;
            world_nodes_[node].children[1] = right;
            world_nodes_[node].count = 0;
        }
        return node;
    }

    void PerformanceMapper::TraceWorld(
        uint32_t node,
        ncl_math::Vector3 origin,
        ncl_math::Vector3 direction,
        float& distance,
        const goldsrc::msurface_t*& surface
    ) const
    {
        const WorldNode& bounds = world_nodes_[node];
        float enter = distance;
        if (!RayBox(origin, direction, bounds.bounds, enter))
        {
            return;
        }
        if (!bounds.count)
        {
            TraceWorld(bounds.children[0], origin, direction, distance, surface);
            TraceWorld(bounds.children[1], origin, direction, distance, surface);
            return;
        }
        for (uint32_t index = bounds.first; index < bounds.first + bounds.count; ++index)
        {
            const goldsrc::msurface_t& candidate = world_->surfaces[world_faces_[index].index];
            if (RaySurface(*world_, candidate, origin, direction, distance))
            {
                surface = &candidate;
            }
        }
    }

    bool PerformanceMapper::CaptureView(int wpoly, int epoly, std::span<goldsrc::cl_entity_t* const> visible, const goldsrc::refdef_t& view)
    {
        int area = view.vrect.width * view.vrect.height;
        if (!snapshot_.enabled || !snapshot_.map_loaded || area <= 0 || area < view_area_ || view.onlyClientDraws)
        {
            return false;
        }
        view_area_ = area;
        snapshot_.view_measured = true;
        snapshot_.wpoly = uint64_t(std::max(0, wpoly));
        snapshot_.epoly = uint64_t(std::max(0, epoly)) + shader_triangles_;
        snapshot_.angles = ncl_math::Vector3::Ref(view.viewangles);
        snapshot_.pvs_brush_entities = 0;
        snapshot_.target_hit = false;
        snapshot_.target_bounds = false;
        snapshot_.target_origin = {};
        snapshot_.target_name.clear();
        snapshot_.properties.clear();
        snapshot_.outline.clear();

        ++snapshot_.revision;
        ncl_math::Vector3 origin = ncl_math::Vector3::Ref(view.vieworg);
        ncl_math::Vector3 direction;
        ncl_math::AngleVectors(view.viewangles, direction.data(), nullptr, nullptr);
        float distance = kTraceDistance;
        const goldsrc::cl_entity_t* target = nullptr;
        const goldsrc::msurface_t* surface = nullptr;
        if (!world_nodes_.empty())
        {
            TraceWorld(0, origin, direction, distance, surface);
            snapshot_.target_hit = surface != nullptr;
        }
        for (const goldsrc::cl_entity_t* entity : visible)
        {
            if (!entity || !entity->model || entity->model == world_)
            {
                continue;
            }
            const goldsrc::model_t& model = *entity->model;
            snapshot_.pvs_brush_entities += model.type == goldsrc::mod_brush;
            ncl_math::Vector3 forward;
            ncl_math::Vector3 right;
            ncl_math::Vector3 up;
            ncl_math::Vector3 angles = ncl_math::Vector3::Ref(entity->angles);
            if (model.type == goldsrc::mod_studio)
            {
                angles.x = -angles.x;
            }
            ncl_math::AngleVectors(angles.data(), forward.data(), right.data(), up.data());
            ncl_math::Vector3 delta = origin - ncl_math::Vector3::Ref(entity->origin);
            ncl_math::Vector3 local_origin{delta.Dot(forward), -delta.Dot(right), delta.Dot(up)};
            ncl_math::Vector3 local_direction{direction.Dot(forward), -direction.Dot(right), direction.Dot(up)};
            ncl_math::Aabb3 bounds = ModelBounds(*entity);
            float hit = distance;
            if (!RayBox(local_origin, local_direction, bounds, hit))
            {
                continue;
            }
            const goldsrc::msurface_t* hit_surface = nullptr;
            if (model.type == goldsrc::mod_brush)
            {
                hit = distance;
                for (int index = model.firstmodelsurface; index < model.firstmodelsurface + model.nummodelsurfaces; ++index)
                {
                    const goldsrc::msurface_t& candidate = model.surfaces[index];
                    if (candidate.numedges >= 3 && RaySurface(model, candidate, local_origin, local_direction, hit))
                    {
                        hit_surface = &candidate;
                    }
                }
            }
            if (hit < distance && (hit_surface || model.type != goldsrc::mod_brush) && hit > kPlaneEpsilon)
            {
                distance = hit;
                target = entity;
                surface = hit_surface;
                snapshot_.target_hit = true;
                snapshot_.target_bounds = model.type != goldsrc::mod_brush;
            }
        }
        if (snapshot_.target_hit)
        {
            snapshot_.target_position = origin + direction * distance;
            CaptureTarget(target, surface);
        }
        return true;
    }

    void PerformanceMapper::CaptureOutline(const goldsrc::cl_entity_t* entity, const goldsrc::msurface_t* surface)
    {
        const goldsrc::model_t& model = entity ? *entity->model : *world_;
        ncl_math::Vector3 origin;
        ncl_math::Vector3 forward{1, 0, 0};
        ncl_math::Vector3 right{0, -1, 0};
        ncl_math::Vector3 up{0, 0, 1};
        if (entity)
        {
            origin = ncl_math::Vector3::Ref(entity->origin);
            ncl_math::Vector3 angles = ncl_math::Vector3::Ref(entity->angles);
            if (model.type == goldsrc::mod_studio)
            {
                angles.x = -angles.x;
            }
            ncl_math::AngleVectors(angles.data(), forward.data(), right.data(), up.data());
        }
        auto world_point = [&](ncl_math::Vector3 point) { return origin + forward * point.x - right * point.y + up * point.z; };
        auto append_surface = [&](const goldsrc::msurface_t& polygon) {
            for (int edge = 0; edge < polygon.numedges; ++edge)
            {
                snapshot_.outline.push_back(
                    {world_point(SurfaceVertex(model, polygon, edge)),
                     world_point(SurfaceVertex(model, polygon, (edge + 1) % polygon.numedges))}
                );
            }
        };
        if (!entity)
        {
            append_surface(*surface);
        }
        else if (model.type == goldsrc::mod_brush)
        {
            for (int index = model.firstmodelsurface; index < model.firstmodelsurface + model.nummodelsurfaces; ++index)
            {
                append_surface(model.surfaces[index]);
            }
        }
        else
        {
            ncl_math::Aabb3 bounds = ModelBounds(*entity);
            for (int corner = 0; corner < 8; ++corner)
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (corner & (1 << axis))
                    {
                        continue;
                    }
                    ncl_math::Vector3 start;
                    for (int component = 0; component < 3; ++component)
                    {
                        start[component] = corner & (1 << component) ? bounds.max[component] : bounds.min[component];
                    }
                    ncl_math::Vector3 end = start;
                    end[axis] = bounds.max[axis];
                    snapshot_.outline.push_back({world_point(start), world_point(end)});
                }
            }
        }
    }

    void PerformanceMapper::CaptureTarget(const goldsrc::cl_entity_t* entity, const goldsrc::msurface_t* surface)
    {
        if (snapshot_.level < 2)
        {
            return;
        }

        const goldsrc::model_t& model = entity ? *entity->model : *world_;
        const MapEntity* metadata = nullptr;
        snapshot_.target_origin = entity ? ncl_math::Vector3::Ref(entity->origin) : ncl_math::Vector3{};
        for (const MapEntity& candidate : map_entities_)
        {
            if ((!entity && candidate.classname == "worldspawn") || (entity && model.name[0] == '*' && candidate.model == model.name))
            {
                if (metadata)
                {
                    metadata = nullptr;
                    break;
                }
                metadata = &candidate;
            }
        }
        snapshot_.target_name = metadata && !metadata->classname.empty() ? metadata->classname : entity ? model.name : "worldspawn";
        if (metadata && !metadata->targetname.empty())
        {
            snapshot_.target_name += " (" + metadata->targetname + ")";
        }
        CaptureOutline(entity, surface);
        snapshot_.properties.push_back({"index", std::to_string(entity ? entity->index : 0)});
        snapshot_.properties.push_back({"model", model.name});
        if (surface && surface->texinfo && surface->texinfo->texture)
        {
            const char* name = surface->texinfo->texture->name;
            snapshot_.properties.push_back({"texture", std::string(name, strnlen(name, 16))});
        }
        if (entity)
        {
            snapshot_.properties.push_back({"origin", Coordinates(snapshot_.target_origin)});
            snapshot_.properties.push_back({"angles", Coordinates(ncl_math::Vector3::Ref(entity->angles))});
            snapshot_.properties.push_back({"render mode", std::to_string(entity->curstate.rendermode)});
            snapshot_.properties.push_back({"render amount", std::to_string(entity->curstate.renderamt)});
            snapshot_.properties.push_back({"effects", std::to_string(entity->curstate.effects)});
            snapshot_.properties.push_back({"skin", std::to_string(entity->curstate.skin)});
            snapshot_.properties.push_back({"body", std::to_string(entity->curstate.body)});
            snapshot_.properties.push_back({"sequence", std::to_string(entity->curstate.sequence)});
            snapshot_.properties.push_back({"solid", std::to_string(entity->curstate.solid)});
        }
        if (metadata)
        {
            for (const PerfMapProperty& property : metadata->properties)
            {
                snapshot_.properties.push_back({"bsp." + property.name, property.value});
            }
        }
    }

    const PerfMapperSnapshot& PerformanceMapper::get_snapshot() const
    {
        return snapshot_;
    }

} // namespace performance
