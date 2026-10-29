#include "Model.h"

#include <filesystem>
#include <fstream>

#include <ncl_studio/studio_mesh_builder.h>

namespace matgen
{
    bool ReadFile(const std::string& path, std::vector<uint8_t>& out)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file)
        {
            return false;
        }

        std::streamoff size = file.tellg();

        if (size < 0)
        {
            return false;
        }

        out.resize(static_cast<size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(out.data()), size);

        return static_cast<bool>(file);
    }

    std::string LoadModel(const std::string& path, Model& out)
    {
        out.path = path;

        if (!ReadFile(path, out.data))
        {
            return "cannot read " + path;
        }

        ncl_utils::ResultT<ncl_studio::StudioMesh> mesh = ncl_studio::BuildStudioMesh(out.data.data(), out.data.size());

        if (mesh.has_error())
        {
            return path + ": " + mesh.error_str();
        }

        out.mesh = *mesh;

        ncl_utils::ResultT<ncl_studio::StudioTextures> textures = ncl_studio::ReadStudioTextures(out.data.data(), out.data.size());

        if (textures.has_error())
        {
            return path + ": " + textures.error_str();
        }

        if (!textures->textures.empty())
        {
            out.textures = *textures;
            out.texture_path = path;

            return {};
        }

        // The compiler splits textures into <name>T.mdl when asked to, and a model whose
        // textures went there declares none of its own. Nothing in the geometry says so, so
        // the companion is looked for by name.
        std::filesystem::path companion = path;
        companion.replace_extension();
        companion += "T.mdl";

        std::vector<uint8_t> companion_data;

        if (!ReadFile(companion.string(), companion_data))
        {
            return path + ": declares no textures, and " + companion.string() + " is not there either";
        }

        ncl_utils::ResultT<ncl_studio::StudioTextures> split = ncl_studio::ReadStudioTextures(companion_data.data(), companion_data.size());

        if (split.has_error())
        {
            return companion.string() + ": " + split.error_str();
        }

        out.textures = *split;
        out.texture_path = companion.string();

        return {};
    }
} // namespace matgen
