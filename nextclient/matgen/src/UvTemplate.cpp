#include "UvTemplate.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <ncl_math/color.h>

#include "Png.h"

namespace matgen
{
    namespace
    {
        constexpr int kMinSize = 64;
        constexpr int kMaxSize = 2048;

        // Bright and fully opaque, because the point of the template is to be visible over
        // whatever the author paints under it.
        constexpr ncl_math::Color kWire = {80, 255, 130, 255};

        void Plot(Image& image, int x, int y, const ncl_math::Color& colour)
        {
            if (x < 0 || y < 0 || x >= image.width || y >= image.height)
            {
                return;
            }

            uint8_t* texel = image.At(x, y);
            texel[0] = colour.r;
            texel[1] = colour.g;
            texel[2] = colour.b;
            texel[3] = colour.a;
        }

        void DrawLine(Image& image, int x0, int y0, int x1, int y1, const ncl_math::Color& colour)
        {
            int dx = std::abs(x1 - x0);
            int dy = -std::abs(y1 - y0);
            int sx = x0 < x1 ? 1 : -1;
            int sy = y0 < y1 ? 1 : -1;
            int error = dx + dy;

            while (true)
            {
                Plot(image, x0, y0, colour);

                if (x0 == x1 && y0 == y1)
                {
                    break;
                }

                int doubled = error * 2;

                if (doubled >= dy)
                {
                    error += dy;
                    x0 += sx;
                }

                if (doubled <= dx)
                {
                    error += dx;
                    y0 += sy;
                }
            }
        }

        std::string SafeName(const std::string& texture_name)
        {
            std::string result;

            for (char c : texture_name)
            {
                // The name comes from the model file and lands in a file name; anything that
                // is not plainly a name becomes an underscore rather than a path element.
                bool plain = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    c == '_' || c == '-';

                result.push_back(plain ? c : '_');
            }

            return result.empty() ? "texture" : result;
        }
    }

    int RunUv(const std::string& model_path, const UvOptions& options)
    {
        Model model;
        std::string error = LoadModel(model_path, model);

        if (!error.empty())
        {
            std::fprintf(stderr, "matgen uv: %s\n", error.c_str());
            return 1;
        }

        if (options.skin_family < 0 || options.skin_family >= model.textures.skins.families)
        {
            std::fprintf(
                stderr,
                "matgen uv: skin family %d, the model has %d\n",
                options.skin_family,
                model.textures.skins.families
            );
            return 1;
        }

        std::filesystem::path directory =
            options.out_dir.empty() ? std::filesystem::path(model_path).parent_path() : std::filesystem::path(options.out_dir);

        if (!directory.empty())
        {
            std::error_code code;
            std::filesystem::create_directories(directory, code);
        }

        std::printf("%s: %zu textures", model_path.c_str(), model.textures.textures.size());

        if (model.texture_path != model.path)
        {
            std::printf(" (from %s)", model.texture_path.c_str());
        }

        std::printf("\n");

        int written = 0;

        for (size_t index = 0; index < model.textures.textures.size(); index++)
        {
            const ncl_studio::TextureInfo& texture = model.textures.textures[index];

            // The template keeps the texture's own proportions: it is drawn over the skin an
            // author is painting, and a square template for a 256x512 skin would stretch
            // every island the author lines up against.
            float scale = options.size > 0
                ? options.size / static_cast<float>(std::max(texture.width, texture.height))
                : 1.0f;

            int out_width = std::clamp(static_cast<int>(texture.width * scale), kMinSize, kMaxSize);
            int out_height = std::clamp(static_cast<int>(texture.height * scale), kMinSize, kMaxSize);

            Image image = MakeImage(out_width, out_height, 4);

            // Texel coordinates divide by the size of the texture the reference resolves to,
            // which is why a template is produced per texture rather than per model: two
            // textures of a model need not be the same size.
            float scale_u = out_width / static_cast<float>(texture.width);
            float scale_v = out_height / static_cast<float>(texture.height);

            int triangles = 0;
            int wrapped = 0;

            float min_u = 0.0f;
            float max_u = 0.0f;
            float min_v = 0.0f;
            float max_v = 0.0f;
            bool bounded = false;

            for (const ncl_studio::MeshRange& mesh : model.mesh.meshes)
            {
                if (model.textures.skins.TextureFor(options.skin_family, mesh.skinref) != static_cast<int32_t>(index))
                {
                    continue;
                }

                for (uint32_t i = 0; i + 2 < mesh.index_count; i += 3)
                {
                    const ncl_studio::Vertex* corner[3] = {
                        &model.mesh.vertices[model.mesh.indices[mesh.first_index + i]],
                        &model.mesh.vertices[model.mesh.indices[mesh.first_index + i + 1]],
                        &model.mesh.vertices[model.mesh.indices[mesh.first_index + i + 2]],
                    };

                    float u[3];
                    float v[3];

                    for (int c = 0; c < 3; c++)
                    {
                        u[c] = corner[c]->texel.x;
                        v[c] = corner[c]->texel.y;

                        min_u = bounded ? std::min(min_u, u[c]) : u[c];
                        max_u = bounded ? std::max(max_u, u[c]) : u[c];
                        min_v = bounded ? std::min(min_v, v[c]) : v[c];
                        max_v = bounded ? std::max(max_v, v[c]) : v[c];
                        bounded = true;
                    }

                    // Islands routinely sit outside the first tile, and the sampler wraps
                    // them back in, so the template has to as well. Shifted per triangle
                    // rather than per corner: wrapping a corner on its own tears a triangle
                    // that happens to straddle the boundary right across the image.
                    float shift_u = std::floor((u[0] + u[1] + u[2]) / 3.0f / texture.width) * texture.width;
                    float shift_v = std::floor((v[0] + v[1] + v[2]) / 3.0f / texture.height) * texture.height;

                    if (shift_u != 0.0f || shift_v != 0.0f)
                    {
                        wrapped++;
                    }

                    for (int edge = 0; edge < 3; edge++)
                    {
                        int next = (edge + 1) % 3;

                        DrawLine(
                            image,
                            static_cast<int>((u[edge] - shift_u) * scale_u),
                            static_cast<int>((v[edge] - shift_v) * scale_v),
                            static_cast<int>((u[next] - shift_u) * scale_u),
                            static_cast<int>((v[next] - shift_v) * scale_v),
                            kWire
                        );
                    }

                    triangles++;
                }
            }

            std::filesystem::path path = directory / (SafeName(texture.name) + "_uv.png");
            std::string write_error = WritePng(path.string(), image);

            if (!write_error.empty())
            {
                std::fprintf(stderr, "matgen uv: %s\n", write_error.c_str());
                return 1;
            }

            std::printf(
                "  %-24s %4dx%-4d %5d triangles  u %.0f..%.0f  v %.0f..%.0f -> %s\n",
                texture.name.c_str(),
                texture.width,
                texture.height,
                triangles,
                min_u,
                max_u,
                min_v,
                max_v,
                path.string().c_str()
            );

            // Said out loud because it changes what the author is looking at: those islands
            // are drawn where the sampler reads them, not where the file put them.
            if (wrapped > 0)
            {
                std::printf("      %d triangles lie outside the texture and were wrapped back into it\n", wrapped);
            }

            written++;
        }

        if (written == 0)
        {
            std::fprintf(stderr, "matgen uv: the model names no textures\n");
            return 1;
        }

        return 0;
    }
}
