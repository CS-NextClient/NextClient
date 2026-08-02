#include "BakeEdges.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <vector>

#include "Png.h"

namespace matgen
{
    namespace
    {
        constexpr int kMinSize = 64;
        constexpr int kMaxSize = 2048;

        // How far the baked value is pushed past the edge of an island. Filtering at a
        // silhouette samples outside the triangles that were drawn, and without this it
        // samples the background, which reads as a hard line of wear along every seam.
        constexpr int kBleedPixels = 8;

        // How far one standard deviation of curvature moves the threshold away from the
        // middle. Curvature is measured against the model's own distribution rather than
        // against zero: on a closed surface almost every corner is somewhat convex, so an
        // absolute measure comes out as a nearly black map that wears everywhere at once.
        constexpr float kSpread = 0.35f;

        // Vertices are emitted per command-list entry, so one corner of the model appears
        // many times. Curvature is a property of the corner rather than of a copy of it, so
        // the copies are merged by exact position; they come from one array in the file and
        // are bit-identical.
        struct PositionKey
        {
            float x, y, z;

            bool operator<(const PositionKey& other) const
            {
                if (x != other.x) { return x < other.x; }
                if (y != other.y) { return y < other.y; }

                return z < other.z;
            }
        };

        struct Corner
        {
            ncl_math::Vector3 position {0.0f, 0.0f, 0.0f};
            ncl_math::Vector3 normal {0.0f, 0.0f, 0.0f};
            float neighbour_sum = 0.0f;
            int neighbours = 0;
        };

        ncl_math::Vector3 Normalised(const ncl_math::Vector3& v)
        {
            float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

            return length > 1e-6f ? ncl_math::Vector3 {v.x / length, v.y / length, v.z / length} : v;
        }

        float Dot(const ncl_math::Vector3& a, const ncl_math::Vector3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        std::map<PositionKey, size_t> BuildCorners(const ncl_studio::StudioMesh& mesh, std::vector<Corner>& corners)
        {
            std::map<PositionKey, size_t> index_of;

            for (const ncl_studio::Vertex& vertex : mesh.vertices)
            {
                PositionKey key {vertex.position.x, vertex.position.y, vertex.position.z};

                std::map<PositionKey, size_t>::iterator found = index_of.find(key);

                if (found == index_of.end())
                {
                    found = index_of.emplace(key, corners.size()).first;
                    corners.push_back(Corner {vertex.position});
                }

                Corner& corner = corners[found->second];
                corner.normal.x += vertex.normal.x;
                corner.normal.y += vertex.normal.y;
                corner.normal.z += vertex.normal.z;
            }

            for (Corner& corner : corners)
            {
                corner.normal = Normalised(corner.normal);
            }

            // Every edge of every triangle, both ways: a corner's curvature is what its
            // neighbours do relative to its own normal, and the direction the edge was
            // stored in has nothing to do with it.
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
            {
                for (int edge = 0; edge < 3; edge++)
                {
                    const ncl_studio::Vertex& from = mesh.vertices[mesh.indices[i + edge]];
                    const ncl_studio::Vertex& to = mesh.vertices[mesh.indices[i + (edge + 1) % 3]];

                    size_t a = index_of[PositionKey {from.position.x, from.position.y, from.position.z}];
                    size_t b = index_of[PositionKey {to.position.x, to.position.y, to.position.z}];

                    if (a == b)
                    {
                        continue;
                    }

                    ncl_math::Vector3 offset {
                        corners[b].position.x - corners[a].position.x,
                        corners[b].position.y - corners[a].position.y,
                        corners[b].position.z - corners[a].position.z,
                    };

                    corners[a].neighbour_sum += Dot(Normalised(offset), corners[a].normal);
                    corners[a].neighbours++;
                }
            }

            return index_of;
        }

        float ConvexityOf(const Corner& corner)
        {
            return corner.neighbours > 0 ? corner.neighbour_sum / corner.neighbours : 0.0f;
        }

        // The model's own middle and spread of curvature, so the map uses the whole range
        // whatever the model's overall shape happens to be.
        struct Distribution
        {
            float mean = 0.0f;
            float deviation = 1.0f;
        };

        Distribution Measure(const std::vector<Corner>& corners)
        {
            Distribution result;
            int count = 0;
            double sum = 0.0;

            for (const Corner& corner : corners)
            {
                if (corner.neighbours > 0)
                {
                    sum += ConvexityOf(corner);
                    count++;
                }
            }

            if (count == 0)
            {
                return result;
            }

            result.mean = static_cast<float>(sum / count);

            double variance = 0.0;

            for (const Corner& corner : corners)
            {
                if (corner.neighbours > 0)
                {
                    double offset = ConvexityOf(corner) - result.mean;
                    variance += offset * offset;
                }
            }

            result.deviation = static_cast<float>(std::sqrt(variance / count));

            if (result.deviation < 1e-5f)
            {
                result.deviation = 1.0f;
            }

            return result;
        }

        // Convex where the neighbours fall away behind the normal, concave where they rise
        // in front of it. Dark is what wears first, so a convex corner becomes a low value.
        float ThresholdOf(const Corner& corner, const Distribution& distribution)
        {
            float normalised = (ConvexityOf(corner) - distribution.mean) / distribution.deviation;

            return std::clamp(0.5f + kSpread * normalised, 0.0f, 1.0f);
        }

        struct Canvas
        {
            int width = 0;
            int height = 0;
            std::vector<float> value;
            std::vector<uint8_t> covered;
        };

        void RasteriseTriangle(
            Canvas& canvas,
            const float uv[3][2],
            const float threshold[3]
        )
        {
            float min_x = std::min({uv[0][0], uv[1][0], uv[2][0]});
            float max_x = std::max({uv[0][0], uv[1][0], uv[2][0]});
            float min_y = std::min({uv[0][1], uv[1][1], uv[2][1]});
            float max_y = std::max({uv[0][1], uv[1][1], uv[2][1]});

            int x0 = std::max(0, static_cast<int>(std::floor(min_x)));
            int x1 = std::min(canvas.width - 1, static_cast<int>(std::ceil(max_x)));
            int y0 = std::max(0, static_cast<int>(std::floor(min_y)));
            int y1 = std::min(canvas.height - 1, static_cast<int>(std::ceil(max_y)));

            float area = (uv[1][0] - uv[0][0]) * (uv[2][1] - uv[0][1]) -
                         (uv[2][0] - uv[0][0]) * (uv[1][1] - uv[0][1]);

            if (std::fabs(area) < 1e-8f)
            {
                return;
            }

            for (int y = y0; y <= y1; y++)
            {
                for (int x = x0; x <= x1; x++)
                {
                    float px = x + 0.5f;
                    float py = y + 0.5f;

                    float w0 = ((uv[1][0] - px) * (uv[2][1] - py) - (uv[2][0] - px) * (uv[1][1] - py)) / area;
                    float w1 = ((uv[2][0] - px) * (uv[0][1] - py) - (uv[0][0] - px) * (uv[2][1] - py)) / area;
                    float w2 = 1.0f - w0 - w1;

                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                    {
                        continue;
                    }

                    size_t at = static_cast<size_t>(y) * canvas.width + x;
                    canvas.value[at] = w0 * threshold[0] + w1 * threshold[1] + w2 * threshold[2];
                    canvas.covered[at] = 1;
                }
            }
        }

        void Bleed(Canvas& canvas, int pixels)
        {
            for (int pass = 0; pass < pixels; pass++)
            {
                std::vector<float> next = canvas.value;
                std::vector<uint8_t> next_covered = canvas.covered;

                for (int y = 0; y < canvas.height; y++)
                {
                    for (int x = 0; x < canvas.width; x++)
                    {
                        size_t at = static_cast<size_t>(y) * canvas.width + x;

                        if (canvas.covered[at])
                        {
                            continue;
                        }

                        float sum = 0.0f;
                        int count = 0;

                        for (int dy = -1; dy <= 1; dy++)
                        {
                            for (int dx = -1; dx <= 1; dx++)
                            {
                                int nx = x + dx;
                                int ny = y + dy;

                                if (nx < 0 || ny < 0 || nx >= canvas.width || ny >= canvas.height)
                                {
                                    continue;
                                }

                                size_t neighbour = static_cast<size_t>(ny) * canvas.width + nx;

                                if (canvas.covered[neighbour])
                                {
                                    sum += canvas.value[neighbour];
                                    count++;
                                }
                            }
                        }

                        if (count > 0)
                        {
                            next[at] = sum / count;
                            next_covered[at] = 1;
                        }
                    }
                }

                canvas.value = std::move(next);
                canvas.covered = std::move(next_covered);
            }
        }
    }

    int RunBakeEdges(const std::string& model_path, const BakeOptions& options)
    {
        Model model;
        std::string error = LoadModel(model_path, model);

        if (!error.empty())
        {
            std::fprintf(stderr, "matgen bake edges: %s\n", error.c_str());
            return 1;
        }

        if (options.skin_family < 0 || options.skin_family >= model.textures.skins.families)
        {
            std::fprintf(
                stderr,
                "matgen bake edges: skin family %d, the model has %d\n",
                options.skin_family,
                model.textures.skins.families
            );
            return 1;
        }

        std::vector<Corner> corners;
        std::map<PositionKey, size_t> index_of = BuildCorners(model.mesh, corners);
        Distribution distribution = Measure(corners);

        std::filesystem::path directory = options.out_dir.empty()
            ? std::filesystem::path(model_path).parent_path()
            : std::filesystem::path(options.out_dir);

        if (!directory.empty())
        {
            std::error_code code;
            std::filesystem::create_directories(directory, code);
        }

        std::printf("%s: %zu corners\n", model_path.c_str(), corners.size());

        for (size_t index = 0; index < model.textures.textures.size(); index++)
        {
            const ncl_studio::TextureInfo& texture = model.textures.textures[index];

            // The texture's own proportions, because the map is sampled by the same
            // coordinates the skin is.
            float scale = options.size > 0
                ? options.size / static_cast<float>(std::max(texture.width, texture.height))
                : 1.0f;

            Canvas canvas;
            canvas.width = std::clamp(static_cast<int>(texture.width * scale), kMinSize, kMaxSize);
            canvas.height = std::clamp(static_cast<int>(texture.height * scale), kMinSize, kMaxSize);

            size_t texels = static_cast<size_t>(canvas.width) * canvas.height;

            // Half rather than zero: an uncovered texel belongs to no triangle and therefore
            // to no surface, and it has to be a threshold that says nothing rather than one
            // that says "wears immediately".
            canvas.value.assign(texels, 0.5f);
            canvas.covered.assign(texels, 0);

            float scale_u = canvas.width / static_cast<float>(texture.width);
            float scale_v = canvas.height / static_cast<float>(texture.height);

            int triangles = 0;

            for (const ncl_studio::MeshRange& mesh : model.mesh.meshes)
            {
                if (model.textures.skins.TextureFor(options.skin_family, mesh.skinref) != static_cast<int32_t>(index))
                {
                    continue;
                }

                for (uint32_t i = 0; i + 2 < mesh.index_count; i += 3)
                {
                    float uv[3][2];
                    float threshold[3];

                    for (int corner = 0; corner < 3; corner++)
                    {
                        const ncl_studio::Vertex& vertex =
                            model.mesh.vertices[model.mesh.indices[mesh.first_index + i + corner]];

                        uv[corner][0] = vertex.texel.x;
                        uv[corner][1] = vertex.texel.y;

                        PositionKey key {vertex.position.x, vertex.position.y, vertex.position.z};
                        threshold[corner] = ThresholdOf(corners[index_of[key]], distribution);
                    }

                    // Islands routinely sit outside the first tile and the sampler wraps
                    // them back in, so the bake has to land where the sampler will read it.
                    // Per triangle rather than per corner, or one straddling the boundary
                    // is torn across the image.
                    float shift_u =
                        std::floor((uv[0][0] + uv[1][0] + uv[2][0]) / 3.0f / texture.width) * texture.width;
                    float shift_v =
                        std::floor((uv[0][1] + uv[1][1] + uv[2][1]) / 3.0f / texture.height) * texture.height;

                    for (int corner = 0; corner < 3; corner++)
                    {
                        uv[corner][0] = (uv[corner][0] - shift_u) * scale_u;
                        uv[corner][1] = (uv[corner][1] - shift_v) * scale_v;
                    }

                    RasteriseTriangle(canvas, uv, threshold);
                    triangles++;
                }
            }

            if (triangles == 0)
            {
                std::printf("  %-24s no triangles use it, skipped\n", texture.name.c_str());
                continue;
            }

            Bleed(canvas, kBleedPixels);

            Image image = MakeImage(canvas.width, canvas.height, 1);

            for (size_t at = 0; at < canvas.value.size(); at++)
            {
                image.pixels[at] = static_cast<uint8_t>(std::clamp(canvas.value[at], 0.0f, 1.0f) * 255.0f + 0.5f);
            }

            std::string name;
            for (char c : texture.name)
            {
                bool plain = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    c == '_' || c == '-';

                name.push_back(plain ? c : '_');
            }

            std::filesystem::path path = directory / (name + "_edges.png");
            std::string write_error = WritePng(path.string(), image);

            if (!write_error.empty())
            {
                std::fprintf(stderr, "matgen bake edges: %s\n", write_error.c_str());
                return 1;
            }

            std::printf("  %-24s %5d triangles -> %s\n", texture.name.c_str(), triangles, path.string().c_str());
        }

        return 0;
    }
}
