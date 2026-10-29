#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <goldsrc/fwd.h>
#include <ncl_math/aabb.h>

namespace performance
{
    struct PerfMapProperty
    {
        std::string name;
        std::string value;
    };

    struct PerfMapLine
    {
        ncl_math::Vector3 start;
        ncl_math::Vector3 end;
    };

    struct PerfMapperSnapshot
    {
        bool enabled{};
        int level{};
        bool map_loaded{};
        bool map_entities_available{};
        bool view_measured{};
        bool target_hit{};
        bool target_bounds{};
        uint64_t revision{};
        uint64_t wpoly{};
        uint64_t epoly{};
        uint32_t brush_entities{};
        uint32_t pvs_brush_entities{};
        ncl_math::Vector3 position;
        ncl_math::Vector3 angles;
        ncl_math::Vector3 target_position;
        ncl_math::Vector3 target_origin;
        std::string target_name;
        std::vector<PerfMapProperty> properties;
        std::vector<PerfMapLine> outline;
    };

    class PerformanceMapper
    {
    private:
        struct MapEntity
        {
            std::string model;
            std::string classname;
            std::string targetname;
            std::vector<PerfMapProperty> properties;
        };

        struct WorldFace
        {
            int index{};
            ncl_math::Aabb3 bounds;
        };

        struct WorldNode
        {
            ncl_math::Aabb3 bounds;
            uint32_t first{};
            uint32_t count{};
            uint32_t children[2]{};
        };

        PerfMapperSnapshot snapshot_;
        std::vector<MapEntity> map_entities_;
        std::vector<WorldFace> world_faces_;
        std::vector<WorldNode> world_nodes_;
        const goldsrc::model_t* world_{};
        uint64_t shader_triangles_{};
        uint32_t map_generation_{};
        int view_area_{};
        bool dump_key_down_{};

    public:
        void SetLevel(int level, const goldsrc::client_static_t* client = nullptr, const goldsrc::server_t* server = nullptr);
        bool is_enabled() const;
        bool HandleKey(int key, bool down, bool gameplay);
        void ResetInput();
        void Reset();
        void BeginFrame();
        void BeginView(int& wpoly, int& epoly);
        bool CaptureView(int wpoly, int epoly, std::span<goldsrc::cl_entity_t* const> visible, const goldsrc::refdef_t& view);
        void AddShaderTriangles(uint64_t triangles);
        void UpdateWorld(const goldsrc::client_state_t* client, uint32_t map_generation);
        const PerfMapperSnapshot& get_snapshot() const;

    private:
        void ParseEntities(const char* text);
        void BuildWorldFaces();
        uint32_t BuildWorldNode(uint32_t first, uint32_t count);
        void TraceWorld(
            uint32_t node,
            ncl_math::Vector3 origin,
            ncl_math::Vector3 direction,
            float& distance,
            const goldsrc::msurface_t*& surface
        ) const;
        void CaptureOutline(const goldsrc::cl_entity_t* entity, const goldsrc::msurface_t* surface);
        void CaptureTarget(const goldsrc::cl_entity_t* entity, const goldsrc::msurface_t* surface);
    };

    PerformanceMapper& GetMapper();
} // namespace performance
