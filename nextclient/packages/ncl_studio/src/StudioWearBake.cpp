#include "ncl_studio/StudioWearBake.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <vector>

#include "ncl_studio/StudioReferencePose.h"
#include "ncl_studio/studio_mesh_builder.h"

namespace ncl_studio
{
    namespace
    {
        constexpr int kMinSize = 64;
        constexpr int kMaxSize = 2048;
        constexpr float kMinDirectionLength = 1e-6f;
        constexpr char kCanceledError[] = "wear generation canceled";

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
        // copies are merged by exact model-space position after applying the reference pose.
        struct PositionKey
        {
            float x, y, z;

            bool operator<(const PositionKey& other) const
            {
                if (x != other.x)
                {
                    return x < other.x;
                }
                if (y != other.y)
                {
                    return y < other.y;
                }

                return z < other.z;
            }
        };

        struct Corner
        {
            ncl_math::Vector3 position{};
            ncl_math::Vector3 normal{};
            float neighbor_sum{};
            int neighbors{};
        };

        ncl_math::Vector3 Normalized(const ncl_math::Vector3& v)
        {
            float length = v.Length();

            return length > kMinDirectionLength ? v / length : v;
        }

        std::map<PositionKey, size_t> BuildCorners(
            const ncl_studio::StudioMesh& mesh,
            std::vector<Corner>& corners,
            const std::stop_token& stop
        )
        {
            std::map<PositionKey, size_t> index_of;

            for (const ncl_studio::Vertex& vertex : mesh.vertices)
            {
                if (stop.stop_requested())
                {
                    return {};
                }

                PositionKey key{vertex.position.x, vertex.position.y, vertex.position.z};

                std::map<PositionKey, size_t>::iterator found = index_of.find(key);

                if (found == index_of.end())
                {
                    found = index_of.emplace(key, corners.size()).first;
                    corners.push_back(Corner{vertex.position});
                }

                Corner& corner = corners[found->second];
                corner.normal = corner.normal + vertex.normal;
            }

            for (Corner& corner : corners)
            {
                if (stop.stop_requested())
                {
                    return {};
                }

                corner.normal = Normalized(corner.normal);
            }

            // Curvature samples each triangle's outgoing edge against the corner normal.
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
            {
                if (stop.stop_requested())
                {
                    return {};
                }

                for (int edge = 0; edge < 3; edge++)
                {
                    const ncl_studio::Vertex& from = mesh.vertices[mesh.indices[i + edge]];
                    const ncl_studio::Vertex& to = mesh.vertices[mesh.indices[i + (edge + 1) % 3]];

                    size_t a = index_of[PositionKey{from.position.x, from.position.y, from.position.z}];
                    size_t b = index_of[PositionKey{to.position.x, to.position.y, to.position.z}];

                    if (a == b)
                    {
                        continue;
                    }

                    ncl_math::Vector3 offset = corners[b].position - corners[a].position;

                    corners[a].neighbor_sum += Normalized(offset).Dot(corners[a].normal);
                    corners[a].neighbors++;
                }
            }

            return index_of;
        }

        float ConvexityOf(const Corner& corner)
        {
            return corner.neighbors > 0 ? corner.neighbor_sum / corner.neighbors : 0.0f;
        }

        // The model's own middle and spread of curvature, so the map uses the whole range
        // whatever the model's overall shape happens to be.
        struct Distribution
        {
            float mean{};
            float deviation = 1.0f;
        };

        Distribution Measure(const std::vector<Corner>& corners, const std::stop_token& stop)
        {
            Distribution result;
            int count = 0;
            double sum = 0.0;

            for (const Corner& corner : corners)
            {
                if (stop.stop_requested())
                {
                    return result;
                }

                if (corner.neighbors > 0)
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
                if (stop.stop_requested())
                {
                    return result;
                }

                if (corner.neighbors > 0)
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

        // Convex where the neighbors fall away behind the normal, concave where they rise
        // in front of it. Dark is what wears first, so a convex corner becomes a low value.
        float ThresholdOf(const Corner& corner, const Distribution& distribution)
        {
            float normalized = (ConvexityOf(corner) - distribution.mean) / distribution.deviation;

            return std::clamp(0.5f + kSpread * normalized, 0.0f, 1.0f);
        }

        struct Canvas
        {
            int width{};
            int height{};
            std::vector<float> value;
            std::vector<uint8_t> covered;
            // Predecessor links are initialized only for pixels marked in raster_revision.
            std::vector<int> predecessor;
            std::vector<uint32_t> written_revision;
            uint32_t raster_revision{};
        };

        int FindUnwritten(Canvas& canvas, size_t row_start, int x)
        {
            int root = x;

            while (root >= 0 && canvas.written_revision[row_start + root] == canvas.raster_revision)
            {
                root = canvas.predecessor[row_start + root];
            }

            while (x != root && x >= 0)
            {
                int next = canvas.predecessor[row_start + x];
                canvas.predecessor[row_start + x] = root;
                x = next;
            }

            return root;
        }

        void RasterizeTriangle(Canvas& canvas, const float uv[3][2], const float threshold[3], const std::stop_token& stop)
        {
            double min_y = std::min({uv[0][1], uv[1][1], uv[2][1]});
            double max_y = std::max({uv[0][1], uv[1][1], uv[2][1]});
            double area = (double(uv[1][0]) - uv[0][0]) * (double(uv[2][1]) - uv[0][1]) -
                          (double(uv[2][0]) - uv[0][0]) * (double(uv[1][1]) - uv[0][1]);

            if (std::fabs(area) < 1e-8)
            {
                return;
            }

            int y0 = static_cast<int>(std::ceil(min_y - 0.5));
            int y1 = static_cast<int>(std::floor(max_y - 0.5));

            if (++canvas.raster_revision == 0)
            {
                std::fill(canvas.written_revision.begin(), canvas.written_revision.end(), 0);
                ++canvas.raster_revision;
            }

            size_t remaining = canvas.value.size();

            // Reverse traversal retains the last repeated sample's interpolation.
            // Predecessor links skip covered intervals across repeated source rows.
            for (int y = y1; y >= y0; y--)
            {
                if (stop.stop_requested())
                {
                    return;
                }

                int wrapped_y = (y % canvas.height + canvas.height) % canvas.height;
                size_t row_start = static_cast<size_t>(wrapped_y) * canvas.width;

                if (FindUnwritten(canvas, row_start, canvas.width - 1) < 0)
                {
                    continue;
                }

                double py = y + 0.5;
                double left = std::numeric_limits<double>::infinity();
                double right = -std::numeric_limits<double>::infinity();

                for (int edge = 0; edge < 3; edge++)
                {
                    int next = (edge + 1) % 3;
                    double dy = double(uv[next][1]) - uv[edge][1];

                    if (dy == 0.0)
                    {
                        continue;
                    }

                    double t = (py - uv[edge][1]) / dy;

                    if (t < 0.0 || t > 1.0)
                    {
                        continue;
                    }

                    double x = uv[edge][0] + t * (double(uv[next][0]) - uv[edge][0]);
                    left = std::min(left, x);
                    right = std::max(right, x);
                }

                if (left > right)
                {
                    continue;
                }

                int x0 = static_cast<int>(std::ceil(left - 0.5));
                int x1 = static_cast<int>(std::floor(right - 0.5));
                x0 = std::max(x0, x1 - canvas.width + 1);

                if (x0 > x1)
                {
                    continue;
                }

                for (int x = x1; x >= x0; x--)
                {
                    int wrapped_x = (x % canvas.width + canvas.width) % canvas.width;
                    int unwritten_x = FindUnwritten(canvas, row_start, wrapped_x);

                    if (unwritten_x < 0)
                    {
                        x -= wrapped_x;
                        continue;
                    }

                    x -= wrapped_x - unwritten_x;

                    if (x < x0)
                    {
                        break;
                    }

                    size_t at = row_start + unwritten_x;

                    double px = x + 0.5;
                    double w0 = ((uv[1][0] - px) * (uv[2][1] - py) - (uv[2][0] - px) * (uv[1][1] - py)) / area;
                    double w1 = ((uv[2][0] - px) * (uv[0][1] - py) - (uv[0][0] - px) * (uv[2][1] - py)) / area;
                    double w2 = 1.0 - w0 - w1;

                    canvas.value[at] = static_cast<float>(w0 * threshold[0] + w1 * threshold[1] + w2 * threshold[2]);
                    canvas.covered[at] = 1;
                    canvas.predecessor[at] = FindUnwritten(canvas, row_start, unwritten_x - 1);
                    canvas.written_revision[at] = canvas.raster_revision;

                    if (--remaining == 0)
                    {
                        return;
                    }
                }
            }
        }

        void Bleed(Canvas& canvas, int pixels, const std::stop_token& stop)
        {
            for (int pass = 0; pass < pixels; pass++)
            {
                std::vector<float> next = canvas.value;
                std::vector<uint8_t> next_covered = canvas.covered;

                for (int y = 0; y < canvas.height; y++)
                {
                    if (stop.stop_requested())
                    {
                        return;
                    }

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
                                int nx = (x + dx + canvas.width) % canvas.width;
                                int ny = (y + dy + canvas.height) % canvas.height;

                                size_t neighbor = static_cast<size_t>(ny) * canvas.width + nx;

                                if (canvas.covered[neighbor])
                                {
                                    sum += canvas.value[neighbor];
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
    } // namespace

    ncl_utils::ResultT<std::vector<StudioWearMap>> BakeStudioWearMaps(
        const void* model_data,
        size_t model_size,
        const StudioTextures& textures,
        const WearBakeOptions& options
    )
    {
        if (options.stop_token.stop_requested())
        {
            return ncl_utils::ResultError(kCanceledError);
        }

        if (!std::isfinite(options.scratch_strength) || options.scratch_strength < 0.0f || options.scratch_strength > 1.0f)
        {
            return ncl_utils::ResultError("scratch strength must be between 0 and 1");
        }

        if (options.skin_family < 0 || options.skin_family >= textures.skins.families || textures.skins.references < 0 ||
            static_cast<uint64_t>(textures.skins.families) * textures.skins.references != textures.skins.values.size())
        {
            return ncl_utils::ResultError("invalid skin family or skin table");
        }

        if (options.scratch_strength > 0.0f &&
            (options.scratch_width <= 0 || options.scratch_height <= 0 ||
             static_cast<uint64_t>(options.scratch_width) * options.scratch_height * 4 != options.scratch_rgba.size()))
        {
            return ncl_utils::ResultError("scratch image dimensions do not match its RGBA pixels");
        }

        for (const TextureInfo& texture : textures.textures)
        {
            if (texture.width <= 0 || texture.height <= 0)
            {
                return ncl_utils::ResultError("texture declares no size");
            }
        }

        ncl_utils::ResultT<StudioMesh> converted = BuildStudioMesh(model_data, model_size);
        if (options.stop_token.stop_requested())
        {
            return ncl_utils::ResultError(kCanceledError);
        }

        if (converted.has_error())
        {
            return ncl_utils::ResultError(converted.error_str());
        }

        StudioMesh mesh = *converted;
        ncl_utils::ResultT<std::vector<StudioBoneTransform>> pose = ReadStudioReferencePose(model_data, model_size);
        if (pose.has_error())
        {
            return ncl_utils::ResultError(pose.error_str());
        }

        for (Vertex& vertex : mesh.vertices)
        {
            if (options.stop_token.stop_requested())
            {
                return ncl_utils::ResultError(kCanceledError);
            }

            vertex.position = (*pose)[vertex.position_bone].Apply(vertex.position);
            vertex.normal = (*pose)[vertex.normal_bone].ApplyVector(vertex.normal);
            for (int axis = 0; axis < 3; axis++)
            {
                if (!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))
                {
                    return ncl_utils::ResultError("reference geometry contains a nonfinite position or normal");
                }
            }
        }

        std::vector<bool> used(textures.textures.size(), false);

        for (const MeshRange& range : mesh.meshes)
        {
            int32_t texture = textures.skins.TextureFor(options.skin_family, range.skinref);
            if (texture < 0 || static_cast<size_t>(texture) >= used.size())
            {
                return ncl_utils::ResultError("mesh skin reference lies outside the texture table");
            }
            if (range.index_count >= 3)
            {
                used[texture] = true;
            }
        }

        std::vector<bool> selected(used.size(), options.texture_indices.empty());
        for (int index : options.texture_indices)
        {
            if (index < 0 || static_cast<size_t>(index) >= used.size() || !used[index])
            {
                return ncl_utils::ResultError("selected texture is not used by this skin family");
            }

            selected[index] = true;
        }

        std::vector<Corner> corners;
        std::map<PositionKey, size_t> index_of = BuildCorners(mesh, corners, options.stop_token);
        if (options.stop_token.stop_requested())
        {
            return ncl_utils::ResultError(kCanceledError);
        }

        for (const Corner& corner : corners)
        {
            if (!std::isfinite(corner.neighbor_sum))
            {
                return ncl_utils::ResultError("reference geometry produces nonfinite curvature");
            }
        }

        Distribution distribution = Measure(corners, options.stop_token);
        if (options.stop_token.stop_requested())
        {
            return ncl_utils::ResultError(kCanceledError);
        }

        std::vector<StudioWearMap> result;
        for (size_t index = 0; index < textures.textures.size(); index++)
        {
            if (options.stop_token.stop_requested())
            {
                return ncl_utils::ResultError(kCanceledError);
            }

            if (!used[index] || !selected[index])
            {
                continue;
            }

            const ncl_studio::TextureInfo& texture = textures.textures[index];

            // The texture's own proportions, because the map is sampled by the same
            // coordinates the skin is.
            float scale = options.size > 0 ? options.size / static_cast<float>(std::max(texture.width, texture.height)) : 1.0f;

            Canvas canvas;
            canvas.width = static_cast<int>(std::clamp(texture.width * scale, float(kMinSize), float(kMaxSize)));
            canvas.height = static_cast<int>(std::clamp(texture.height * scale, float(kMinSize), float(kMaxSize)));

            size_t texels = static_cast<size_t>(canvas.width) * canvas.height;

            // Half rather than zero: an uncovered texel belongs to no triangle and therefore
            // to no surface, and it has to be a threshold that says nothing rather than one
            // that says "wears immediately".
            canvas.value.assign(texels, 0.5f);
            canvas.covered.assign(texels, 0);
            canvas.predecessor.resize(texels);
            canvas.written_revision.assign(texels, 0);

            float scale_u = canvas.width / static_cast<float>(texture.width);
            float scale_v = canvas.height / static_cast<float>(texture.height);

            int triangles = 0;

            for (const ncl_studio::MeshRange& range : mesh.meshes)
            {
                if (textures.skins.TextureFor(options.skin_family, range.skinref) != static_cast<int32_t>(index))
                {
                    continue;
                }

                for (uint32_t i = 0; i + 2 < range.index_count; i += 3)
                {
                    float uv[3][2];
                    float threshold[3];

                    for (int corner = 0; corner < 3; corner++)
                    {
                        const ncl_studio::Vertex& vertex = mesh.vertices[mesh.indices[range.first_index + i + corner]];

                        uv[corner][0] = vertex.texel.x;
                        uv[corner][1] = vertex.texel.y;

                        PositionKey key{vertex.position.x, vertex.position.y, vertex.position.z};
                        threshold[corner] = ThresholdOf(corners[index_of[key]], distribution);
                    }

                    // A common offset preserves interpolation and whole-tile translation.
                    float shift_u = std::floor(std::min({uv[0][0], uv[1][0], uv[2][0]}) / texture.width) * texture.width;
                    float shift_v = std::floor(std::min({uv[0][1], uv[1][1], uv[2][1]}) / texture.height) * texture.height;

                    for (int corner = 0; corner < 3; corner++)
                    {
                        uv[corner][0] = (uv[corner][0] - shift_u) * scale_u;
                        uv[corner][1] = (uv[corner][1] - shift_v) * scale_v;
                    }

                    RasterizeTriangle(canvas, uv, threshold, options.stop_token);
                    if (options.stop_token.stop_requested())
                    {
                        return ncl_utils::ResultError(kCanceledError);
                    }

                    triangles++;
                }
            }

            if (triangles == 0)
            {
                continue;
            }


            if (options.scratch_strength > 0.0f)
            {
                for (int y = 0; y < canvas.height; y++)
                {
                    if (options.stop_token.stop_requested())
                    {
                        return ncl_utils::ResultError(kCanceledError);
                    }

                    const int source_y = static_cast<int>(static_cast<int64_t>(y) * options.scratch_height / canvas.height);
                    for (int x = 0; x < canvas.width; x++)
                    {
                        const size_t at = static_cast<size_t>(y) * canvas.width + x;
                        if (!canvas.covered[at])
                        {
                            continue;
                        }
                        const int source_x = static_cast<int>(static_cast<int64_t>(x) * options.scratch_width / canvas.width);
                        const size_t source = (static_cast<size_t>(source_y) * options.scratch_width + source_x) * 4;
                        const float scratch = options.scratch_rgba[source] / 255.0f;
                        canvas.value[at] *= 1.0f - options.scratch_strength * (1.0f - scratch);
                    }
                }
            }

            Bleed(canvas, kBleedPixels, options.stop_token);
            if (options.stop_token.stop_requested())
            {
                return ncl_utils::ResultError(kCanceledError);
            }

            StudioWearMap image;
            image.texture_index = static_cast<int>(index);
            image.width = canvas.width;
            image.height = canvas.height;
            image.pixels.resize(canvas.value.size());
            for (size_t at = 0; at < canvas.value.size(); at++)
            {
                image.pixels[at] = static_cast<uint8_t>(std::clamp(canvas.value[at], 0.0f, 1.0f) * 255.0f + 0.5f);
            }
            result.push_back(std::move(image));
        }

        return std::move(result);
    }
} // namespace ncl_studio
