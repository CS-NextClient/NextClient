#pragma once

#include <string>
#include <vector>

#include <ncl_studio/StudioMesh.h>
#include <ncl_studio/StudioTextures.h>

namespace matgen
{
    // A model as the tools need it: the geometry from the model file and the texture table
    // from wherever it actually lives.
    struct Model
    {
        std::string path;
        std::vector<uint8_t> data;

        ncl_studio::StudioMesh mesh;
        ncl_studio::StudioTextures textures;

        // Where the texture table came from, so a message can name the file an author has
        // to look at rather than the one they typed.
        std::string texture_path;
    };

    bool ReadFile(const std::string& path, std::vector<uint8_t>& out);

    // Loads the model and, when its textures were split into the companion file the
    // compiler writes as <name>T.mdl, loads that too. Returns an error message, empty on
    // success.
    std::string LoadModel(const std::string& path, Model& out);
} // namespace matgen
