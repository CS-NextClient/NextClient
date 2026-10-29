#include "graphics/render/material/MaterialCoverageRuntime.h"

#include <glad/glad.h>
#include <ncl_gl/program.h>
#include <ncl_material/MaterialDoc.h>
#include <ncl_material/MaterialResolve.h>

#include "graphics/render/material/MaterialTextureCache.h"
#include "graphics/render/material/MaterialManifestCatalog.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/material/StudioProgramContract.h"
#include "graphics/render/StudioShaderInterface.h"
#include "graphics/render/material/studio_effect_material.h"

render::MaterialCoverageBindings render::MaterialCoverageBindings::Prepare(const ncl_gl::Program& program)
{
    MaterialCoverageBindings bindings;
    const StudioProgramContract& contract = StudioContract_EffectsV1();

    for (size_t index = 0; index < contract.coverage_scalars.size(); ++index)
    {
        bindings.scalars[index] = program.UniformLocation(contract.get_parameter(contract.coverage_scalars[index]).name.c_str());
    }

    bindings.transform = program.UniformLocation(contract.get_parameter(StudioParameterRole::DissolveTransform).name.c_str());
    bindings.viewport = program.UniformLocation(kStudioUniformNames[kUniformEffectViewport]);
    bindings.time = program.UniformLocation(kStudioUniformNames[kUniformEffectTime]);
    bindings.dissolve_map = program.UniformLocation(contract.get_parameter(StudioParameterRole::DissolveMap).name.c_str());

    return bindings;
}

render::MaterialCoverageRuntime::MaterialCoverageRuntime(
    MaterialManifestCatalog& manifests,
    MaterialTextureCache& textures,
    performance::PerformanceMonitor& monitor
) :
    monitor_(monitor),
    manifests_(manifests),
    textures_(textures)
{}

const ncl_material::ProgramManifest* render::MaterialCoverageRuntime::ManifestFor(const ncl_material::Material* material)
{
    return material != nullptr && material->program != ncl_material::kLegacyProgram ? manifests_.Get(material->program) : nullptr;
}

bool render::MaterialCoverageRuntime::PrepareImages(
    const ncl_material::Material* material,
    const ncl_material::ProgramManifest* manifest,
    StudioMeshImages& images,
    tex::TextureLoadGuard guard
)
{
    images.manifest = manifest;
    images.coverage_dissolve_map = 0;
    const StudioProgramContract* contract = StudioContract_For(material, manifest);
    if (contract == nullptr)
    {
        return true;
    }
    const float dissolve = effects::Scalar(material, manifest, StudioParameterRole::Dissolve);
    if (dissolve <= 0.0f || dissolve >= 1.0f)
    {
        return true;
    }
    const ncl_material::ParamValue* map =
        effects::Parameter(material, manifest, contract->get_parameter(StudioParameterRole::DissolveMap).name);
    bool canceled = false;
    unsigned int texture = map != nullptr && map->type == ncl_material::ParamType::TextureRef && !map->texture_ref.empty()
                               ? textures_.Get(map->texture_ref.c_str(), guard, &canceled)
                               : 0;
    if (canceled)
    {
        return false;
    }
    if (texture == 0)
    {
        texture = textures_.Neutral(guard, &canceled);
    }
    if (canceled)
    {
        return false;
    }
    images.coverage_dissolve_map = texture;
    return true;
}

uint32_t render::MaterialCoverageRuntime::get_texture_revision() const
{
    return textures_.get_revision();
}

bool render::MaterialCoverageRuntime::is_texture_revision(uint32_t revision) const
{
    return revision == textures_.get_revision();
}

void render::MaterialCoverageRuntime::Upload(
    const MaterialCoverageBindings& bindings,
    const ncl_material::Material* material,
    const ncl_material::ProgramManifest* manifest,
    const std::array<int, 4>& viewport,
    float time,
    StudioMaterialState& textures,
    unsigned int dissolve_image
)
{
    const StudioProgramContract* contract = StudioContract_For(material, manifest);
    if (contract == nullptr)
    {
        material = nullptr;
    }

    // A null material still resets coverage uniforms left by the preceding authored mesh.
    const StudioProgramContract* layout = contract != nullptr ? contract : &StudioContract_EffectsV1();

    float dissolve = 0.0f;

    for (size_t index = 0; index < layout->coverage_scalars.size(); ++index)
    {
        const StudioParameterRole role = layout->coverage_scalars[index];
        const char* name = layout->get_parameter(role).name.c_str();
        const ncl_material::ParamValue* value = effects::Parameter(material, manifest, name);
        float scalar = value != nullptr && value->type == ncl_material::ParamType::Scalar ? value->scalar : 0.0f;
        glUniform1f(bindings.scalars[index], scalar);

        if (role == StudioParameterRole::Dissolve)
        {
            dissolve = scalar;
        }
    }

    const ncl_material::ParamValue* transform =
        effects::Parameter(material, manifest, layout->get_parameter(StudioParameterRole::DissolveTransform).name);
    const std::array<float, 4> identity = {1, 1, 0, 0};
    glUniform4fv(
        bindings.transform,
        1,
        transform != nullptr && transform->type == ncl_material::ParamType::Vec4 ? transform->vec4.data() : identity.data()
    );
    glUniform4f(
        bindings.viewport,
        static_cast<float>(viewport[0]),
        static_cast<float>(viewport[1]),
        static_cast<float>(viewport[2]),
        static_cast<float>(viewport[3])
    );
    glUniform1f(bindings.time, time);

    if (dissolve > 0.0f && dissolve < 1.0f)
    {
        GLint location = bindings.dissolve_map;

        if (location < 0)
        {
            return;
        }

        textures.BindTexture(0, dissolve_image, monitor_);
        glUniform1i(location, kFirstMaterialUnit);
        glActiveTexture(GL_TEXTURE0 + kStudioBaseTextureUnit);
    }
}
