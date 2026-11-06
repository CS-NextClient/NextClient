#include "graphics/render/material/material_manifests.h"

#include "engine.h"
#include "console/console.h"
#include "graphics/render/render_resources.h"

namespace render
{
    namespace
    {
        class NativeManifestBackend final : public ManifestBackend
        {
        public:
            void RegisterLocalization(const char* path) override
            {
                if (g_pLocalize != nullptr)
                {
                    g_pLocalize->AddFile(g_pFileSystem, path);
                }
            }
            void ReportError(const std::string& program, const std::string& error) override
            {
                Con_Printf("render: manifest of program %s: %s\n", program.c_str(), error.c_str());
            }
        };
    } // namespace

    ManifestBackend& GetManifestBackend()
    {
        static NativeManifestBackend backend;
        return backend;
    }

    const ncl_material::ProgramManifest* Manifest_Get(const std::string& program)
    {
        return GetResources().manifests.Get(program);
    }

    const ncl_material::ParamValue* Manifest_GetParam(const ncl_material::Material* material, std::string_view name)
    {
        return GetResources().manifests.GetParam(material, name);
    }
} // namespace render
