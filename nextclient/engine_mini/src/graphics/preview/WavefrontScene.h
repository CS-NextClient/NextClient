#pragma once

#include <string>
#include <vector>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/WavefrontParser.h"
#include "graphics/preview/PreviewAssetBackend.h"
#include "graphics/render/RenderDevice.h"

namespace render::preview
{
    // Triangles gathered by texture, and the textures themselves, which stand in the engine's
    // texture list under names of the scene's own.
    class WavefrontScene : public render::IDeviceResource
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
        friend class WavefrontSceneTestAccess;

        RenderDevice& device_;
        PreviewAssetBackend& assets_;
        std::string path_;
        std::vector<std::string> wads_;
        int tag_{};
        bool textures_invalid_{};
        std::vector<Batch> batches_;

    public:
        WavefrontScene(RenderDevice& device, PreviewAssetBackend& assets);
        ~WavefrontScene() override;
        WavefrontScene(const WavefrontScene&) = delete;
        WavefrontScene& operator=(const WavefrontScene&) = delete;
        void OnDeviceLost() override;

        // Archives are read in order and only as far as a texture is still missing; tag tells
        // this scene's textures from another scene's. False leaves the scene empty.
        bool Load(const char* path, const char* const* wads, int wad_count, int tag);
        bool Reload();
        void Clear();
        void Draw(const PreviewLight& light);

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

} // namespace render::preview
