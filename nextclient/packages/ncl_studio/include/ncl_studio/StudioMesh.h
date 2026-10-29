#pragma once

#include <cstdint>
#include <vector>

#include <ncl_math/vec3.h>

namespace ncl_studio
{
    inline constexpr int kStudioVersion = 10;


    struct Vertex
    {
        ncl_math::Vector3 position;
        ncl_math::Vector3 normal;

        // The glow shell moves each vertex out along a normal, and the engine uses one
        // normal per vertex of the file for it: the first the command lists name, which is
        // what BuildNormalIndexTable records. A vertex on a hard edge is emitted here once
        // per normal it is drawn with, so extruding each copy along its own normal pulls
        // them apart and tears the shell open along every such edge.
        ncl_math::Vector3 shell_normal;

        // Kept in texel space on purpose. The divisor is the size of the texture the
        // active skin family resolves to, which is picked per entity at draw time and
        // may differ between families, so normalizing here would bake in one family.
        ncl_math::Vector2 texel;

        uint8_t position_bone;
        uint8_t normal_bone;
        uint8_t shell_normal_bone;
    };

    struct MeshRange
    {
        uint32_t first_index;
        uint32_t index_count;
        int32_t skinref;
    };

    struct SubModelRange
    {
        uint32_t first_mesh;
        uint32_t mesh_count;

        // Byte offset of the mstudiomodel_t inside the file, so a submodel pointer
        // handed out by the engine at draw time maps back to this range.
        uint32_t file_offset;
    };

    struct BodyPartRange
    {
        uint32_t first_submodel;
        uint32_t submodel_count;
    };

    // One studio model with its strip and fan command lists expanded into indexed
    // triangles. Everything lands in a single vertex and index array so the whole model
    // occupies one pair of GPU buffers and each mesh is drawn as a range.
    struct StudioMesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<MeshRange> meshes;
        std::vector<SubModelRange> submodels;
        std::vector<BodyPartRange> bodyparts;
    };
} // namespace ncl_studio
