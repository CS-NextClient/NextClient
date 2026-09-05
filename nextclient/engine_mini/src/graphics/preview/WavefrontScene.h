#pragma once

#include <string>
#include <vector>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/WavefrontParser.h"

// Triangles gathered by texture, and the textures themselves, which stand in the engine's
// texture list under names of the scene's own.
class WavefrontScene
{
    // The triangles drawn with one texture, three vertices each.
    struct Batch
    {
        std::string texture;
        std::string identifier;
        int texnum = -1;
        std::vector<WavefrontVertex> vertices;
    };

private:
    std::string path_;
    std::vector<std::string> wads_;
    int tag_{};
    std::vector<Batch> batches_;

public:
    ~WavefrontScene();

    // Archives are read in order and only as far as a texture is still missing; tag tells
    // this scene's textures from another scene's. False leaves the scene empty.
    bool Load(const char* path, const char* const* wads, int wad_count, int tag);
    bool Reload();
    void Draw(const PreviewLight& light) const;

    bool is_empty() const
    {
        return batches_.empty();
    }

private:
    bool Read();
    void LoadTextures();
    void LoadTexturesFrom(const char* wad_name);
    void Free();
};
