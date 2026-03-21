#pragma once
#include "resource/texture_manager/Texture.h"
#include "gl_local.h"

namespace tex
{
    /// Loads a TGA image from gfx/<filename>.tga and uploads it as an OpenGL texture.
    /// Populates texture with the resulting GL object metadata.
    /// Returns false if the file cannot be loaded.
    bool LoadTextureFromFile(Texture& texture, const TexIdentifierStr& identifier, const char* filename, bool mipmap, int filter);

    /// Main texture upload pipeline. Handles indexed (palette-based), RGBA and grayscale
    /// formats, optional POT resampling, hardware palette path, mipmap generation and
    /// GL filter setup. The resulting GL object metadata is written to texture.
    /// palette is required for indexed formats, ignored otherwise.
    /// Returns false on validation or unpack failure.
    bool LoadTexture(
        Texture& texture,
        const TexIdentifierStr& identifier,
        TextureFormat format,
        const uint8_t* data,
        int width,
        int height,
        bool mipmap,
        const uint8_t* palette,
        int filter
    );

    /// Deletes the GL texture object owned by texture and releases
    /// the associated hardware palette, if any.
    void UnloadTexture(Texture& texture);
} // namespace tex
