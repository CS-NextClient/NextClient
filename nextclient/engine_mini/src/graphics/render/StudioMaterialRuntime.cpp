#include "graphics/render/StudioMaterialRuntime.h"

#include <algorithm>
#include <cassert>

#include <ncl_material/MaterialResolve.h>
#include <ncl_material/ProgramManifest.h>

#include "performance/PerformanceMonitor.h"
#include "performance/performance_scopes.h"

namespace render
{
    namespace
    {
        const std::string kRigidDefines = std::string(kStudioShadeDefines) + "\n#define STUDIO_RIGID_INSTANCING 1\n";
        const ProgramDesc kRigidProgram{
            "studio.vert",
            "studio_legacy.frag",
            kRigidDefines.c_str(),
            kStudioUniformNames,
            kUniformCount,
            nullptr,
            0,
        };
    } // namespace

    static_assert(kStudioMaterialUnits == ncl_material::kMaxSchemaTextures);
    static_assert(kStudioMaterialUnits <= sizeof(uint32_t) * 8);

    void StudioMaterialState::BindTexture(int slot, GLuint texture, performance::PerformanceMonitor& monitor)
    {
        assert(slot >= 0 && slot < kStudioMaterialUnits);
        glActiveTexture(GL_TEXTURE0 + kFirstMaterialUnit + slot);
        uint32_t bit = 1u << slot;

        if (!(captured_ & bit))
        {
            performance::LocalCpuScope cpu(monitor.get_studio_detail_monitor(), performance::PerfStage::StudioGlSave);
            performance::LocalCpuScope capture(monitor.get_studio_detail_monitor(), performance::PerfStage::StudioBatchStateSave);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_[slot]);
            captured_ |= bit;
        }

        glBindTexture(GL_TEXTURE_2D, texture);
    }

    void StudioMaterialState::Restore()
    {
        for (int slot = 0; slot < kStudioMaterialUnits; ++slot)
        {
            if (captured_ & (1u << slot))
            {
                glActiveTexture(GL_TEXTURE0 + kFirstMaterialUnit + slot);
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_[slot]));
            }
        }

        captured_ = 0;
    }

    StudioMaterialRuntime::StudioMaterialRuntime(
        ShaderManager& programs,
        MaterialManifestCatalog& manifests,
        MaterialTextureCache& textures,
        performance::PerformanceMonitor& monitor
    ) :
        monitor_(monitor),
        programs_(programs),
        manifests_(manifests),
        textures_(textures)
    {}

    StudioMaterialRuntime::MaterialProgram& StudioMaterialRuntime::FindOrCreateMaterialProgram(const std::string& program)
    {
        for (const std::unique_ptr<MaterialProgram>& entry : material_programs_)
        {
            if (entry->name == program)
            {
                return *entry;
            }
        }

        std::unique_ptr<MaterialProgram> entry = std::make_unique<MaterialProgram>();
        entry->name = program;
        entry->vertex = program + ".vert";
        entry->fragment = program + ".frag";
        entry->desc = ProgramDesc{
            entry->vertex.c_str(), entry->fragment.c_str(), kStudioShadeDefines, kStudioUniformNames, kUniformCount, nullptr, 0
        };

        material_programs_.push_back(std::move(entry));

        return *material_programs_.back();
    }

    const ncl_gl::Program* StudioMaterialRuntime::ProgramFor(
        const ncl_material::Material* material,
        const StudioMaterialBindings** bindings
    )
    {
        uint32_t generation = programs_.get_generation();

        if (material == nullptr || material->program == ncl_material::kLegacyProgram)
        {
            if (builtin_generation_ != generation)
            {
                builtin_generation_ = generation;
                builtin_program_ = programs_.Get(kStudioBuiltinProgram);
            }

            if (bindings != nullptr)
            {
                if (builtin_program_ != nullptr)
                {
                    PrepareBindings(builtin_bindings_, *builtin_program_);
                }

                *bindings = builtin_program_ != nullptr ? &builtin_bindings_ : nullptr;
            }

            return builtin_program_;
        }

        MaterialProgram& entry = FindOrCreateMaterialProgram(material->program);

        if (entry.generation != generation)
        {
            entry.generation = generation;
            entry.program = programs_.Get(entry.desc);
        }

        if (bindings != nullptr)
        {
            if (entry.program != nullptr)
            {
                PrepareBindings(entry, *entry.program);
            }

            *bindings = entry.program != nullptr ? &entry.bindings : nullptr;
        }

        return entry.program;
    }

    const StudioMaterialBindings* StudioMaterialRuntime::PrepareRigidBindings()
    {
        if (rigid_generation_ != programs_.get_generation())
        {
            rigid_generation_ = programs_.get_generation();
            rigid_program_ = programs_.Get(kRigidProgram);
        }

        if (rigid_program_ == nullptr)
        {
            return nullptr;
        }

        PrepareBindings(rigid_bindings_, *rigid_program_);

        return &rigid_bindings_;
    }

    void StudioMaterialRuntime::PrepareBindings(
        StudioMaterialBindings& bindings,
        const ncl_gl::Program& program,
        const ncl_material::Material* material
    )
    {
        const ncl_material::ProgramManifest* manifest =
            material != nullptr && material->program != ncl_material::kLegacyProgram ? manifests_.Get(material->program) : nullptr;
        if (bindings.manifest != manifest)
        {
            bindings.manifest = manifest;
            bindings.generation = 0;
        }

        PrepareBindings(bindings, program);
    }

    void StudioMaterialRuntime::PrepareBindings(MaterialProgram& entry, const ncl_gl::Program& program)
    {
        if (!entry.manifest_resolved)
        {
            entry.bindings.manifest = manifests_.Get(entry.name);
            entry.manifest_resolved = true;
        }

        PrepareBindings(entry.bindings, program);
    }

    void StudioMaterialRuntime::PrepareBindings(StudioMaterialBindings& bindings, const ncl_gl::Program& program)
    {
        const uint32_t generation = programs_.get_generation();

        if (bindings.program == &program && bindings.generation == generation)
        {
            return;
        }

        bindings.program = &program;
        bindings.generation = generation;
        bindings.bones = program.UniformBlockIndex("Bones");
        bindings.texture = program.UniformLocation("uTexture");
        bindings.light_gamma = program.UniformLocation("uLightGamma");
        bindings.parameters.clear();

        if (bindings.manifest != nullptr)
        {
            bindings.parameters.reserve(bindings.manifest->parameters.size());

            for (const ncl_material::ParamSchema& schema : bindings.manifest->parameters)
            {
                bindings.parameters.push_back(program.UniformLocation(schema.name.c_str()));
            }
        }
    }

    void StudioMaterialRuntime::ConfigureProgram(const StudioMaterialBindings& bindings)
    {
        const ncl_gl::Program& program = *bindings.program;
        uint32_t generation = bindings.generation;

        if (configured_program_ == program.prog && configured_generation_ == generation && configured_gamma_revision_ == gamma_revision_)
        {
            return;
        }

        configured_program_ = program.prog;
        configured_generation_ = generation;
        configured_gamma_revision_ = gamma_revision_;
        GLuint block = bindings.bones;

        if (block != GL_INVALID_INDEX)
        {
            glUniformBlockBinding(program.prog, block, kStudioBonesBinding);
        }

        glUniform1i(bindings.texture, kStudioBaseTextureUnit);
        glUniform1fv(bindings.light_gamma, kStudioLightGammaEntries, light_gamma_.data());
    }

    bool StudioMaterialRuntime::PrepareImages(
        const StudioMaterialBindings& bindings,
        const ncl_material::Material& material,
        StudioMeshImages& images,
        tex::TextureLoadGuard guard
    )
    {
        images = StudioMeshImages{};
        images.manifest = bindings.manifest;
        const ncl_material::ProgramManifest* manifest = bindings.manifest;
        if (manifest == nullptr)
        {
            return true;
        }
        for (size_t index = 0; index < manifest->parameters.size(); ++index)
        {
            const ncl_material::ParamSchema& schema = manifest->parameters[index];
            if (schema.type != ncl_material::ParamType::TextureRef || bindings.parameters[index] < 0)
            {
                continue;
            }
            const ncl_material::ParamValue* value = material.FindParam(schema.name);
            if (value == nullptr && schema.has_default)
            {
                value = &schema.default_value;
            }
            bool canceled = false;
            unsigned int texture = value != nullptr ? textures_.Get(value->texture_ref.c_str(), guard, &canceled) : 0;
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
            assert(images.material_count < kStudioMaterialUnits);
            images.material[images.material_count++] = texture;
        }
        return true;
    }

    int StudioMaterialRuntime::UploadMaterial(
        const StudioMaterialBindings& bindings,
        const ncl_material::Material& material,
        const StudioMeshImages& images,
        StudioMaterialState& state
    )
    {
        performance::LocalCpuScope preparation(monitor_.get_scope_monitor(), performance::PerfStage::StudioMaterial);
        const ncl_material::ProgramManifest* manifest = bindings.manifest;

        if (manifest == nullptr)
        {
            return 0;
        }

        std::array<GLint, kStudioMaterialUnits> locations{};
        int texture_count = 0;

        for (size_t index = 0; index < manifest->parameters.size(); ++index)
        {
            const ncl_material::ParamSchema& schema = manifest->parameters[index];
            GLint location = bindings.parameters[index];

            if (location < 0)
            {
                continue;
            }

            if (schema.type == ncl_material::ParamType::TextureRef)
            {
                assert(texture_count < images.material_count);
                locations[texture_count++] = location;

                continue;
            }

            const ncl_material::ParamValue* value = material.FindParam(schema.name);

            if (value == nullptr && schema.has_default)
            {
                value = &schema.default_value;
            }

            if (value == nullptr)
            {
                continue;
            }

            if (value->type == ncl_material::ParamType::Scalar)
            {
                glUniform1f(location, value->scalar);
            }
            else
            {
                glUniform4fv(location, 1, value->vec4.data());
            }
        }

        for (int slot = 0; slot < texture_count; ++slot)
        {
            state.BindTexture(slot, images.material[slot], monitor_);
            glUniform1i(locations[slot], kFirstMaterialUnit + slot);
        }

        if (texture_count != 0)
        {
            glActiveTexture(GL_TEXTURE0 + kStudioBaseTextureUnit);
        }

        return texture_count;
    }

    void StudioMaterialRuntime::SetLightGamma(std::span<const float, kStudioLightGammaEntries> values)
    {
        std::copy(values.begin(), values.end(), light_gamma_.begin());
        ++gamma_revision_;
    }

    void StudioMaterialRuntime::WarmPrograms()
    {
        std::vector<ProgramDesc> descs;
        descs.push_back(kStudioBuiltinProgram);

        for (const std::string& name : programs_.ListPrograms())
        {
            if (manifests_.Get(name) != nullptr)
            {
                descs.push_back(FindOrCreateMaterialProgram(name).desc);
            }
        }

        programs_.WarmUp(descs.data(), static_cast<int>(descs.size()));
    }

    void StudioMaterialRuntime::InvalidatePrograms()
    {
        builtin_program_ = nullptr;
        builtin_generation_ = 0;
        builtin_bindings_.generation = 0;
        rigid_program_ = nullptr;
        rigid_generation_ = 0;
        rigid_bindings_.generation = 0;
        configured_program_ = 0;
        configured_generation_ = 0;
        configured_gamma_revision_ = 0;

        for (const std::unique_ptr<MaterialProgram>& entry : material_programs_)
        {
            entry->program = nullptr;
            entry->generation = 0;
            entry->bindings.generation = 0;
        }
    }
} // namespace render
