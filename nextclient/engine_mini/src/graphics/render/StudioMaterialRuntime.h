#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <ncl_material/MaterialDoc.h>
#include <ncl_material/ProgramManifest.h>

#include "graphics/render/ShaderManager.h"
#include "graphics/render/StudioBindings.h"
#include "graphics/render/StudioShaderInterface.h"
#include "graphics/render/material/MaterialManifestCatalog.h"
#include "graphics/render/material/MaterialTextureCache.h"
#include "graphics/render/material/StudioMeshImages.h"

namespace performance
{
    class PerformanceMonitor;
}

namespace render
{
    class StudioMaterialState
    {
    private:
        std::array<GLint, kStudioMaterialUnits> previous_{};
        uint32_t captured_{};

    public:
        void BindTexture(int slot, GLuint texture, performance::PerformanceMonitor& monitor);
        void Restore();
    };

    struct StudioMaterialBindings
    {
        const ncl_gl::Program* program{};
        const ncl_material::ProgramManifest* manifest{};
        std::vector<GLint> parameters;
        uint32_t generation{};
        GLuint bones{};
        GLint texture{};
        GLint light_gamma{};
    };

    class StudioMaterialRuntime
    {
    private:
        struct MaterialProgram
        {
            std::string name;
            std::string vertex;
            std::string fragment;
            ProgramDesc desc{};
            const ncl_gl::Program* program{};
            uint32_t generation{};
            StudioMaterialBindings bindings;
            bool manifest_resolved{};
        };

        performance::PerformanceMonitor& monitor_;
        ShaderManager& programs_;
        MaterialManifestCatalog& manifests_;
        MaterialTextureCache& textures_;
        std::vector<std::unique_ptr<MaterialProgram>> material_programs_;
        const ncl_gl::Program* builtin_program_{};
        uint32_t builtin_generation_{};
        StudioMaterialBindings builtin_bindings_;
        const ncl_gl::Program* rigid_program_{};
        uint32_t rigid_generation_{};
        StudioMaterialBindings rigid_bindings_;
        GLuint configured_program_{};
        uint32_t configured_generation_{};
        uint64_t configured_gamma_revision_{};
        uint64_t gamma_revision_ = 1;
        std::array<float, kStudioLightGammaEntries> light_gamma_{};

    public:
        // Resource owners must outlive the runtime; construction performs no GL work.
        StudioMaterialRuntime(
            ShaderManager& programs,
            MaterialManifestCatalog& manifests,
            MaterialTextureCache& textures,
            performance::PerformanceMonitor& monitor
        );

        const ncl_gl::Program* ProgramFor(const ncl_material::Material* material, const StudioMaterialBindings** bindings = nullptr);
        const StudioMaterialBindings* PrepareRigidBindings();
        uint32_t get_shader_generation() const
        {
            return programs_.get_generation();
        }
        void PrepareBindings(StudioMaterialBindings& bindings, const ncl_gl::Program& program, const ncl_material::Material* material);
        void ConfigureProgram(const StudioMaterialBindings& bindings);
        bool PrepareImages(
            const StudioMaterialBindings& bindings,
            const ncl_material::Material& material,
            StudioMeshImages& images,
            tex::TextureLoadGuard guard = {}
        );
        int UploadMaterial(
            const StudioMaterialBindings& bindings,
            const ncl_material::Material& material,
            const StudioMeshImages& images,
            StudioMaterialState& state
        );
        uint32_t get_texture_revision() const
        {
            return textures_.get_revision();
        }
        void SetLightGamma(std::span<const float, kStudioLightGammaEntries> values);
        void WarmPrograms();
        void InvalidatePrograms();

    private:
        void PrepareBindings(MaterialProgram& entry, const ncl_gl::Program& program);
        void PrepareBindings(StudioMaterialBindings& bindings, const ncl_gl::Program& program);
        MaterialProgram& FindOrCreateMaterialProgram(const std::string& name);
    };
} // namespace render
