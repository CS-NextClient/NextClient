#include "graphics/render/render_resources.h"

namespace render
{
    namespace
    {
        class UnusedResourceBackend final : public ShaderBackend, public ManifestBackend, public MaterialImageLoader
        {
        public:
            bool LoadSource(const char*, std::string&) override
            {
                return false;
            }

            std::vector<std::string> ListPrograms() override
            {
                return {};
            }

            bool Build(const ProgramDesc&, ncl_gl::Program&) override
            {
                return false;
            }

            void DeleteProgram(unsigned int) override {}

            double Time() override
            {
                return 0;
            }

            void ReportWarmup(int, int, int, double) override {}
            void RegisterLocalization(const char*) override {}
            void ReportError(const std::string&, const std::string&) override {}

            unsigned int Load(const char*, tex::TextureLoadGuard) override
            {
                return 0;
            }

            unsigned int CreateNeutral() override
            {
                return 0;
            }

            void DeleteTexture(unsigned int) override {}
            void ReportRejected(const char*) override {}
        };
    } // namespace

    RendererResources& GetResources()
    {
        static UnusedResourceBackend backend;
        static RendererResources resources(backend, backend, backend);
        return resources;
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
