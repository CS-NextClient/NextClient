#include "RadarBspFixture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <goldsrc/engine/model.h>
#include <goldsrc/format/bsp.h>

#include "hud/radar/map/MapBspGeometry.h"

namespace
{
    constexpr size_t kMaxFileBytes = 256 * 1024 * 1024;
    constexpr ncl_math::Aabb3 kCrouchBounds{{-16.0f, -16.0f, -18.0f}, {16.0f, 16.0f, 18.0f}};
    constexpr int kPlaneLump = 1, kTextureLump = 2, kVertexLump = 3, kTexinfoLump = 6;
    constexpr int kFaceLump = 7, kClipnodeLump = 9, kEdgeLump = 12, kSurfedgeLump = 13, kModelLump = 14;

    struct Lump
    {
        int offset;
        int length;
    };

    struct Header
    {
        int version;
        Lump lumps[15];
    };

    struct Plane
    {
        float normal[3];
        float distance;
        int type;
    };

    struct Vertex
    {
        float position[3];
    };

    struct Edge
    {
        uint16_t vertex[2];
    };

    struct Face
    {
        uint16_t plane;
        int16_t side;
        int firstedge;
        int16_t numedges;
        int16_t texinfo;
        uint8_t styles[4];
        int lightofs;
    };

    struct Texinfo
    {
        float vectors[2][4];
        int miptex;
        int flags;
    };

    static_assert(sizeof(Header) == 124 && sizeof(Plane) == 20 && sizeof(Face) == 20 && sizeof(Texinfo) == 40);

    bool Fail(std::string& error, const char* message)
    {
        error = message;

        return false;
    }

    template <class T>
    bool Records(std::span<const char> bytes, Lump lump, std::vector<T>& out, std::string& error)
    {
        if ((size_t)lump.length % sizeof(T) != 0)
        {
            return Fail(error, "BSP lump record size is invalid.");
        }

        out.resize((size_t)lump.length / sizeof(T));

        if (!out.empty())
        {
            std::memcpy(out.data(), bytes.data() + lump.offset, (size_t)lump.length);
        }

        return true;
    }

    bool Finite(const float* values, size_t count)
    {
        for (size_t i = 0; i < count; i++)
        {
            if (!std::isfinite(values[i]))
            {
                return false;
            }
        }

        return true;
    }

    bool TextureName(std::span<const char> bytes, const Texinfo& info, std::string& name, std::string& error)
    {
        if (info.miptex < 0 || bytes.empty())
        {
            return true;
        }

        int count = 0;

        if (bytes.size() < sizeof(count))
        {
            return Fail(error, "BSP texture header is truncated.");
        }

        std::memcpy(&count, bytes.data(), sizeof(count));

        if (count < 0 || (size_t)count > (bytes.size() - sizeof(count)) / sizeof(int) || info.miptex >= count)
        {
            return Fail(error, "BSP texture index is invalid.");
        }

        int offset = 0;
        std::memcpy(&offset, bytes.data() + sizeof(int) * (1 + info.miptex), sizeof(offset));

        if (offset == -1)
        {
            return true;
        }

        if (offset < 0 || (size_t)offset > bytes.size() || bytes.size() - offset < 16)
        {
            return Fail(error, "BSP texture name is truncated.");
        }

        const char* start = bytes.data() + offset;
        size_t length = 0;

        while (length < 16 && start[length])
        {
            length++;
        }

        name.assign(start, length);

        return true;
    }
} // namespace

bool RadarBspFixture_Load(const std::filesystem::path& path, RadarMapGeometry& out, std::string& error)
{
    out = RadarMapGeometry{};
    error.clear();
    std::ifstream input(path, std::ios::binary | std::ios::ate);

    if (!input)
    {
        return Fail(error, "Cannot open local BSP.");
    }

    std::streamoff length = input.tellg();

    if (length < (std::streamoff)sizeof(Header) || length > (std::streamoff)kMaxFileBytes)
    {
        return Fail(error, "BSP header/file size is invalid.");
    }

    std::vector<char> bytes((size_t)length);
    input.seekg(0);

    if (!input.read(bytes.data(), (std::streamsize)bytes.size()))
    {
        return Fail(error, "Cannot read local BSP.");
    }

    Header header{};
    std::memcpy(&header, bytes.data(), sizeof(header));

    if (header.version != goldsrc::HLBSP_VERSION)
    {
        return Fail(error, "Unsupported BSP version.");
    }

    for (const Lump& lump : header.lumps)
    {
        if (lump.offset < 0 || lump.length < 0 || (size_t)lump.offset > bytes.size() || (size_t)lump.length > bytes.size() - lump.offset)
        {
            return Fail(error, "BSP lump range is invalid.");
        }
    }

    std::vector<Plane> planes;
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Texinfo> texinfo;
    std::vector<int> surfedges;
    std::vector<goldsrc::dclipnode_t> clipnodes;
    std::vector<goldsrc::dmodel_t> models;

    if (!Records(bytes, header.lumps[kPlaneLump], planes, error) || !Records(bytes, header.lumps[kVertexLump], vertices, error) ||
        !Records(bytes, header.lumps[kEdgeLump], edges, error) || !Records(bytes, header.lumps[kFaceLump], faces, error) ||
        !Records(bytes, header.lumps[kTexinfoLump], texinfo, error) || !Records(bytes, header.lumps[kSurfedgeLump], surfedges, error) ||
        !Records(bytes, header.lumps[kClipnodeLump], clipnodes, error) || !Records(bytes, header.lumps[kModelLump], models, error))
    {
        return false;
    }

    if (models.empty())
    {
        return Fail(error, "BSP world model is missing.");
    }

    const goldsrc::dmodel_t& model = models[0];

    if (!Finite(model.mins, 3) || !Finite(model.maxs, 3) || model.firstface < 0 || model.numfaces <= 0 ||
        (size_t)model.firstface > faces.size() || (size_t)model.numfaces > faces.size() - model.firstface ||
        (model.headnode[3] >= 0 && (size_t)model.headnode[3] >= clipnodes.size()))
    {
        return Fail(error, "BSP world bounds/surface/hull range is invalid.");
    }

    for (int axis = 0; axis < 3; axis++)
    {
        if (model.mins[axis] > model.maxs[axis])
        {
            return Fail(error, "BSP world bounds are reversed.");
        }
    }

    std::vector<goldsrc::mplane_t> abi_planes(planes.size());

    for (size_t i = 0; i < planes.size(); i++)
    {
        if (!Finite(planes[i].normal, 3) || !std::isfinite(planes[i].distance))
        {
            return Fail(error, "BSP plane is nonfinite.");
        }

        std::memcpy(abi_planes[i].normal, planes[i].normal, sizeof(planes[i].normal));
        abi_planes[i].dist = planes[i].distance;
    }

    for (const goldsrc::dclipnode_t& node : clipnodes)
    {
        if (node.planenum < 0 || (size_t)node.planenum >= planes.size())
        {
            return Fail(error, "BSP clipnode plane is invalid.");
        }

        for (short child : node.children)
        {
            if (child >= 0 && (size_t)child >= clipnodes.size())
            {
                return Fail(error, "BSP clipnode child is invalid.");
            }
        }
    }

    std::vector<goldsrc::mvertex_t> abi_vertices(vertices.size());

    for (size_t i = 0; i < vertices.size(); i++)
    {
        if (!Finite(vertices[i].position, 3))
        {
            return Fail(error, "BSP vertex is nonfinite.");
        }

        std::memcpy(abi_vertices[i].position, vertices[i].position, sizeof(vertices[i].position));
    }

    std::vector<goldsrc::medge_t> abi_edges(edges.size());

    for (size_t i = 0; i < edges.size(); i++)
    {
        if (edges[i].vertex[0] >= vertices.size() || edges[i].vertex[1] >= vertices.size())
        {
            return Fail(error, "BSP edge vertex is invalid.");
        }

        std::memcpy(abi_edges[i].v, edges[i].vertex, sizeof(edges[i].vertex));
    }

    for (int edge : surfedges)
    {
        if (edge == std::numeric_limits<int>::min() || (size_t)std::abs(edge) >= edges.size())
        {
            return Fail(error, "BSP surfedge is invalid.");
        }
    }

    std::vector<goldsrc::msurface_t> abi_surfaces(faces.size());
    std::vector<std::unique_ptr<std::byte[]>> polygons(faces.size());
    const Lump& textures = header.lumps[kTextureLump];

    for (size_t i = 0; i < faces.size(); i++)
    {
        const Face& face = faces[i];

        if (face.plane >= planes.size() || face.texinfo < 0 || (size_t)face.texinfo >= texinfo.size() || face.numedges < 3 ||
            face.firstedge < 0 || (size_t)face.firstedge > surfedges.size() || (size_t)face.numedges > surfedges.size() - face.firstedge)
        {
            return Fail(error, "BSP face references are invalid.");
        }

        const Texinfo& info = texinfo[face.texinfo];

        if (!Finite(info.vectors[0], 8))
        {
            return Fail(error, "BSP texture vectors are nonfinite.");
        }

        std::string texture_name;

        if (!TextureName({bytes.data() + textures.offset, (size_t)textures.length}, info, texture_name, error))
        {
            return false;
        }

        goldsrc::msurface_t& surface = abi_surfaces[i];
        surface.plane = &abi_planes[face.plane];
        surface.firstedge = face.firstedge;
        surface.numedges = face.numedges;

        if (face.side)
        {
            surface.flags |= goldsrc::SURF_PLANEBACK;
        }

        if (info.flags & 1)
        {
            surface.flags |= goldsrc::SURF_DRAWTILED;
        }

        if (texture_name.starts_with("sky"))
        {
            surface.flags |= goldsrc::SURF_DRAWSKY;
        }

        size_t polygon_bytes = sizeof(goldsrc::glpoly_t) + (size_t)std::max(0, face.numedges - 4) * sizeof(float[7]);
        polygons[i] = std::make_unique<std::byte[]>(polygon_bytes);
        goldsrc::glpoly_t* polygon = reinterpret_cast<goldsrc::glpoly_t*>(polygons[i].get());
        polygon->numverts = face.numedges;
        surface.polys = polygon;

        for (int j = 0; j < face.numedges; j++)
        {
            int edge = surfedges[face.firstedge + j];
            const Vertex& vertex = vertices[edges[std::abs(edge)].vertex[edge < 0 ? 1 : 0]];
            float* values = reinterpret_cast<float*>(polygons[i].get() + offsetof(goldsrc::glpoly_t, verts) + (size_t)j * sizeof(float[7]));
            std::memcpy(values, vertex.position, sizeof(vertex.position));
            // Offline analysis needs deterministic affine UVs, without engine lightmaps.
            for (int axis = 0; axis < 2; axis++)
            {
                const float* vector = info.vectors[axis];
                float coordinate = vector[0] * values[0] + vector[1] * values[1] + vector[2] * values[2] + vector[3];
                values[3 + axis] = coordinate / 128.0f;
                values[5 + axis] = coordinate / 16.0f;
            }
        }
    }

    const Lump& entity_lump = header.lumps[0];
    std::string entities(bytes.data() + entity_lump.offset, (size_t)entity_lump.length);

    goldsrc::model_t world{};
    world.type = goldsrc::mod_brush;
    world.entities = entities.data();
    world.planes = abi_planes.data();
    world.numplanes = (int)abi_planes.size();
    world.vertexes = abi_vertices.data();
    world.numvertexes = (int)abi_vertices.size();
    world.edges = abi_edges.data();
    world.numedges = (int)abi_edges.size();
    world.surfedges = surfedges.data();
    world.numsurfedges = (int)surfedges.size();
    world.surfaces = abi_surfaces.data();
    world.numsurfaces = (int)abi_surfaces.size();
    world.firstmodelsurface = model.firstface;
    world.nummodelsurfaces = model.numfaces;
    world.numclipnodes = (int)clipnodes.size();
    std::memcpy(world.mins, model.mins, sizeof(model.mins));
    std::memcpy(world.maxs, model.maxs, sizeof(model.maxs));
    goldsrc::hull_t& hull = world.hulls[3];
    hull.planes = abi_planes.data();
    hull.clipnodes = clipnodes.data();
    hull.firstclipnode = model.headnode[3];
    hull.lastclipnode = (int)clipnodes.size() - 1;
    std::memcpy(hull.clip_mins, kCrouchBounds.min.data(), sizeof(hull.clip_mins));
    std::memcpy(hull.clip_maxs, kCrouchBounds.max.data(), sizeof(hull.clip_maxs));

    if (!MapBspGeometry_Collect(world, hull, kCrouchBounds, out))
    {
        return Fail(error, "BSP geometry adapter rejected the fixture.");
    }

    return true;
}
