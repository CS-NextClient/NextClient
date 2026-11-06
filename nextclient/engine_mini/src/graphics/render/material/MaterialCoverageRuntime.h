#pragma once

#include <array>

#include <ncl_material/ProgramManifest.h>

#include "graphics/render/material/StudioProgramContract.h"
#include "graphics/render/material/StudioMeshImages.h"
#include "graphics/TextureLoadGuard.h"

namespace ncl_gl
{
    struct Program;
}

namespace performance
{
    class PerformanceMonitor;
}

namespace render
{
    class MaterialManifestCatalog;
    class MaterialTextureCache;
    class StudioMaterialState;

    struct MaterialCoverageBindings
    {
        std::array<int, std::tuple_size_v<decltype(StudioProgramContract::coverage_scalars)>> scalars{};
        int transform{};
        int viewport{};
        int time{};
        int dissolve_map{};

        static MaterialCoverageBindings Prepare(const ncl_gl::Program& program);
    };

    class MaterialCoverageRuntime
    {
    private:
        performance::PerformanceMonitor& monitor_;
        MaterialManifestCatalog& manifests_;
        MaterialTextureCache& textures_;

    public:
        MaterialCoverageRuntime(
            MaterialManifestCatalog& manifests,
            MaterialTextureCache& textures,
            performance::PerformanceMonitor& monitor
        );
        const ncl_material::ProgramManifest* ManifestFor(const ncl_material::Material* material);
        bool PrepareImages(
            const ncl_material::Material* material,
            const ncl_material::ProgramManifest* manifest,
            StudioMeshImages& images,
            tex::TextureLoadGuard guard = {}
        );
        bool is_texture_revision(uint32_t revision) const;
        uint32_t get_texture_revision() const;
        void Upload(
            const MaterialCoverageBindings& bindings,
            const ncl_material::Material* material,
            const ncl_material::ProgramManifest* manifest,
            const std::array<int, 4>& viewport,
            float time,
            StudioMaterialState& textures,
            unsigned int dissolve_image
        );
    };
} // namespace render
