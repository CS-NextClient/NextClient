#include "bake_edges.h"

#include <cstdio>
#include <filesystem>
#include <vector>

#include <ncl_studio/StudioWearBake.h>

#include "Png.h"
#include "texture_output_name.h"

namespace matgen
{
    int RunBakeEdges(const std::string& model_path, const BakeOptions& options)
    {
        Model model;
        std::string error = LoadModel(model_path, model);
        if (!error.empty())
        {
            std::fprintf(stderr, "matgen bake edges: %s\n", error.c_str());
            return 1;
        }

        ncl_studio::WearBakeOptions bake_options;
        bake_options.size = options.size;
        bake_options.skin_family = options.skin_family;
        auto baked = ncl_studio::BakeStudioWearMaps(model.data.data(), model.data.size(), model.textures, bake_options);
        if (baked.has_error())
        {
            std::fprintf(stderr, "matgen bake edges: %s\n", baked.error_str().c_str());
            return 1;
        }

        std::vector<std::string> filenames(model.textures.textures.size());
        for (const ncl_studio::StudioWearMap& map : *baked)
        {
            filenames[map.texture_index] = TextureOutputName(model.textures.textures[map.texture_index].name) + "_edges.png";
        }
        error = CheckTextureOutputNames(filenames);
        if (!error.empty())
        {
            std::fprintf(stderr, "matgen bake edges: %s\n", error.c_str());
            return 1;
        }

        std::filesystem::path directory =
            options.out_dir.empty() ? std::filesystem::path(model_path).parent_path() : std::filesystem::path(options.out_dir);
        if (!directory.empty())
        {
            std::error_code code;
            std::filesystem::create_directories(directory, code);
        }

        for (const ncl_studio::StudioWearMap& map : *baked)
        {
            Image image;
            image.width = map.width;
            image.height = map.height;
            image.channels = 1;
            image.pixels = map.pixels;
            const std::filesystem::path path = directory / filenames[map.texture_index];
            error = WritePng(path.string(), image);
            if (!error.empty())
            {
                std::fprintf(stderr, "matgen bake edges: %s\n", error.c_str());
                return 1;
            }
            std::printf("  %-24s -> %s\n", model.textures.textures[map.texture_index].name.c_str(), path.string().c_str());
        }
        return 0;
    }
} // namespace matgen
