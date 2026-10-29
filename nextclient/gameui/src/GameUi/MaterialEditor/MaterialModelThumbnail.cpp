#include "GameUi/MaterialEditor/MaterialModelThumbnail.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <goldsrc/format/studio.h>
#include <ncl_math/aabb.h>
#include <ncl_math/vec3.h>
#include <ncl_studio/studio_mesh_builder.h>
#include <ncl_studio/StudioReferencePose.h>
#include <ncl_studio/StudioTextures.h>
#include <ncl_studio/studio_validate.h>

namespace material_editor
{

namespace
{
    constexpr int kImageSize = 128;
    constexpr size_t kMaxSourceBytes = 32 * 1024 * 1024;
    constexpr size_t kMaxIndices = 750000;
    constexpr size_t kMaxRasterCandidates = 4 * 1024 * 1024;

    struct RasterVertex
    {
        ncl_math::Vector3 position;
        ncl_math::Vector2 texel;
        float light;
    };

    bool IsFinite(const ncl_math::Vector3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    float Edge(const ncl_math::Vector3& a, const ncl_math::Vector3& b, float x, float y)
    {
        return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
    }

    int Wrap(float value, int size)
    {
        int texel = static_cast<int>(std::fmod(std::floor(value), static_cast<float>(size)));
        return texel < 0 ? texel + size : texel;
    }

    bool Rasterize(
        const RasterVertex& a,
        const RasterVertex& b,
        const RasterVertex& c,
        const goldsrc::mstudiotexture_t& texture,
        std::span<const uint8_t> data,
        MaterialModelThumbnail& image,
        std::vector<float>& depth,
        size_t& candidates_left
    )
    {
        float area = Edge(a.position, b.position, c.position.x, c.position.y);

        if (std::abs(area) < 0.001f)
        {
            return true;
        }

        int x0 = std::clamp(static_cast<int>(std::floor(std::min({a.position.x, b.position.x, c.position.x}))), 0, kImageSize - 1);
        int y0 = std::clamp(static_cast<int>(std::floor(std::min({a.position.y, b.position.y, c.position.y}))), 0, kImageSize - 1);
        int x1 = std::clamp(static_cast<int>(std::ceil(std::max({a.position.x, b.position.x, c.position.x}))), 0, kImageSize - 1);
        int y1 = std::clamp(static_cast<int>(std::ceil(std::max({a.position.y, b.position.y, c.position.y}))), 0, kImageSize - 1);
        size_t candidates = static_cast<size_t>(x1 - x0 + 1) * (y1 - y0 + 1);

        if (candidates > candidates_left)
        {
            return false;
        }

        candidates_left -= candidates;
        const uint8_t* pixels = data.data() + texture.index;
        const uint8_t* palette = pixels + static_cast<size_t>(texture.width) * texture.height;

        for (int y = y0; y <= y1; y++)
        {
            for (int x = x0; x <= x1; x++)
            {
                float u = Edge(b.position, c.position, x + 0.5f, y + 0.5f) / area;
                float v = Edge(c.position, a.position, x + 0.5f, y + 0.5f) / area;
                float w = 1.0f - u - v;

                if (std::min({u, v, w}) < 0.0f)
                {
                    continue;
                }

                float z = a.position.z * u + b.position.z * v + c.position.z * w;
                size_t output = static_cast<size_t>(y) * kImageSize + x;

                if (z <= depth[output])
                {
                    continue;
                }

                ncl_math::Vector2 uv = a.texel * u + b.texel * v + c.texel * w;
                int color = pixels[static_cast<size_t>(Wrap(uv.y, texture.height)) * texture.width + Wrap(uv.x, texture.width)];

                if ((texture.flags & goldsrc::STUDIO_NF_MASKED) != 0 && color == 255)
                {
                    continue;
                }

                float light = std::clamp(a.light * u + b.light * v + c.light * w, 0.35f, 1.0f);
                depth[output] = z;

                for (int channel = 0; channel < 3; channel++)
                {
                    image.rgba[output * 4 + channel] = static_cast<uint8_t>(palette[color * 3 + channel] * light);
                }

                image.rgba[output * 4 + 3] = 255;
            }
        }

        return true;
    }
} // namespace

ncl_utils::ResultT<MaterialModelThumbnail> RenderModelThumbnail(std::span<const uint8_t> model, std::span<const uint8_t> companion)
{
    if (model.size() > kMaxSourceBytes || companion.size() > kMaxSourceBytes)
    {
        return ncl_utils::ResultError("thumbnail source exceeds 32 MiB");
    }

    ncl_utils::Result<> validation = ncl_studio::ValidateStudioFile(model.data(), model.size());

    if (validation.has_error())
    {
        return ncl_utils::ResultError(validation.error_str());
    }

    const goldsrc::studiohdr_t& header = *reinterpret_cast<const goldsrc::studiohdr_t*>(model.data());
    std::span<const uint8_t> texture_data = header.numtextures > 0 ? model : companion;
    validation = ncl_studio::ValidateStudioFile(texture_data.data(), texture_data.size());

    if (validation.has_error())
    {
        return ncl_utils::ResultError(validation.error_str());
    }

    const goldsrc::studiohdr_t& texture_header = *reinterpret_cast<const goldsrc::studiohdr_t*>(texture_data.data());

    if (texture_header.numtextures <= 0 || texture_header.textureindex <= 0)
    {
        return ncl_utils::ResultError("thumbnail requires a validated texture table");
    }

    auto geometry = ncl_studio::BuildStudioMesh(model.data(), model.size());
    auto textures = ncl_studio::ReadStudioTextures(texture_data.data(), texture_data.size());

    if (geometry.has_error() || textures.has_error())
    {
        return ncl_utils::ResultError(geometry.has_error() ? geometry.error_str() : textures.error_str());
    }

    if (textures->textures.empty() || geometry->indices.size() > kMaxIndices)
    {
        return ncl_utils::ResultError("thumbnail needs textures and at most 250000 triangles");
    }

    ncl_utils::ResultT<std::vector<ncl_studio::StudioBoneTransform>> transforms =
        ncl_studio::ReadStudioReferencePose(model.data(), model.size());

    if (transforms.has_error())
    {
        return ncl_utils::ResultError(transforms.error_str());
    }

    ncl_math::Vector3 forward{0.905f, 0.39f, -0.17f};
    ncl_math::Vector3 right = forward.Cross({0, 0, 1}).Normalized();
    ncl_math::Vector3 up = right.Cross(forward).Normalized();
    ncl_math::Aabb2 bounds{
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
        {-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()}
    };
    std::vector<RasterVertex> vertices;
    vertices.reserve(geometry->vertices.size());
    std::vector<uint32_t> selected_meshes;
    std::vector<bool> visible(geometry->vertices.size());

    for (const ncl_studio::BodyPartRange& body : geometry->bodyparts)
    {
        if (body.submodel_count == 0)
        {
            continue;
        }

        const ncl_studio::SubModelRange& submodel = geometry->submodels[body.first_submodel];

        for (uint32_t mesh_index = submodel.first_mesh; mesh_index < submodel.first_mesh + submodel.mesh_count; mesh_index++)
        {
            selected_meshes.push_back(mesh_index);
            const ncl_studio::MeshRange& mesh = geometry->meshes[mesh_index];

            for (uint32_t index = mesh.first_index; index < mesh.first_index + mesh.index_count; index++)
            {
                visible[geometry->indices[index]] = true;
            }
        }
    }

    for (const ncl_studio::Vertex& vertex : geometry->vertices)
    {
        ncl_math::Vector3 position = (*transforms)[vertex.position_bone].Apply(vertex.position);
        ncl_math::Vector3 normal = (*transforms)[vertex.normal_bone].ApplyVector(vertex.normal);
        ncl_math::Vector3 projected{position.Dot(right), -position.Dot(up), -position.Dot(forward)};

        if (!IsFinite(projected) || !IsFinite(normal) || !std::isfinite(vertex.texel.x) || !std::isfinite(vertex.texel.y))
        {
            return ncl_utils::ResultError("nonfinite thumbnail geometry");
        }

        if (visible[vertices.size()])
        {
            bounds.Include({projected.x, projected.y});
        }
        vertices.push_back({projected, vertex.texel, 0.55f + 0.45f * std::abs(normal.Normalized().Dot(forward))});
    }

    ncl_math::Vector2 size = bounds.Size();
    float extent = std::max(size.x, size.y);

    if (vertices.empty() || !std::isfinite(extent) || extent <= 0.0001f)
    {
        return ncl_utils::ResultError("empty thumbnail geometry");
    }

    float scale = (kImageSize - 16.0f) / extent;
    ncl_math::Vector2 center = bounds.min + (bounds.max - bounds.min) * 0.5f;

    for (RasterVertex& vertex : vertices)
    {
        vertex.position.x = (vertex.position.x - center.x) * scale + kImageSize * 0.5f;
        vertex.position.y = (vertex.position.y - center.y) * scale + kImageSize * 0.5f;
    }

    MaterialModelThumbnail image{std::vector<uint8_t>(kImageSize * kImageSize * 4)};
    std::vector<float> depth(kImageSize * kImageSize, -std::numeric_limits<float>::infinity());
    const goldsrc::mstudiotexture_t* texture_table =
        reinterpret_cast<const goldsrc::mstudiotexture_t*>(texture_data.data() + texture_header.textureindex);

    size_t candidates_left = kMaxRasterCandidates;

    for (uint32_t mesh_index : selected_meshes)
    {
        const ncl_studio::MeshRange& mesh = geometry->meshes[mesh_index];
        int texture = textures->skins.TextureFor(0, mesh.skinref);

        if (texture < 0)
        {
            return ncl_utils::ResultError("invalid thumbnail skin reference");
        }

        for (uint32_t index = mesh.first_index; index < mesh.first_index + mesh.index_count; index += 3)
        {
            bool rendered = Rasterize(
                vertices[geometry->indices[index]],
                vertices[geometry->indices[index + 1]],
                vertices[geometry->indices[index + 2]],
                texture_table[texture],
                texture_data,
                image,
                depth,
                candidates_left
            );

            if (!rendered)
            {
                return ncl_utils::ResultError("thumbnail raster budget exceeded");
            }
        }
    }

    return image;
}

} // namespace material_editor
