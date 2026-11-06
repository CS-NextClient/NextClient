#pragma once

#include <string>
#include <vector>

#include "graphics/render/RenderDevice.h"
#include "graphics/TextureLoadGuard.h"

namespace render
{
    class MaterialImageLoader
    {
    public:
        virtual ~MaterialImageLoader() = default;
        virtual unsigned int Load(const char* path, tex::TextureLoadGuard guard) = 0;
        virtual unsigned int CreateNeutral() = 0;
        virtual void DeleteTexture(unsigned int texture) = 0;
        virtual void ReportRejected(const char* path) = 0;
    };

    class MaterialTextureCache final : public IDeviceResource
    {
    private:
        struct Entry
        {
            std::string path;
            unsigned int texture{};
        };

        RenderDevice& device_;
        MaterialImageLoader& loader_;
        std::vector<Entry> textures_;
        unsigned int neutral_{};
        uint32_t revision_ = 1;

    public:
        MaterialTextureCache(RenderDevice& device, MaterialImageLoader& loader);
        ~MaterialTextureCache() override;
        MaterialTextureCache(const MaterialTextureCache&) = delete;
        MaterialTextureCache& operator=(const MaterialTextureCache&) = delete;
        unsigned int Get(const char* path, tex::TextureLoadGuard guard = {}, bool* canceled = nullptr);
        unsigned int Neutral(tex::TextureLoadGuard guard = {}, bool* canceled = nullptr);
        uint32_t get_revision() const
        {
            return revision_;
        }
        const uint32_t& get_revision_source() const
        {
            return revision_;
        }
        void Clear();
        void Forget();
        void OnDeviceLost() override;
    };

    MaterialImageLoader& GetMaterialImageLoader();
} // namespace render
