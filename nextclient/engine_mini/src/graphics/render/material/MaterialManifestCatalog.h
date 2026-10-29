#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <ncl_material/ProgramManifest.h>

namespace render
{
    class ShaderManager;

    class ManifestBackend
    {
    public:
        virtual ~ManifestBackend() = default;
        virtual void RegisterLocalization(const char* path) = 0;
        virtual void ReportError(const std::string& program, const std::string& error) = 0;
    };

    class MaterialManifestCatalog
    {
    private:
        struct Entry;
        ShaderManager& shaders_;
        ManifestBackend& backend_;
        std::vector<std::unique_ptr<Entry>> manifests_;

    public:
        MaterialManifestCatalog(ShaderManager& shaders, ManifestBackend& backend);
        ~MaterialManifestCatalog();
        MaterialManifestCatalog(const MaterialManifestCatalog&) = delete;
        MaterialManifestCatalog& operator=(const MaterialManifestCatalog&) = delete;
        const ncl_material::ProgramManifest* Get(const std::string& program);
        const ncl_material::ParamValue* GetParam(const ncl_material::Material* material, std::string_view name);
    };

    ManifestBackend& GetManifestBackend();
} // namespace render
