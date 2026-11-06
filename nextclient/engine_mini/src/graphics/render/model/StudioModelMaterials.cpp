#include "graphics/render/model/StudioModelMaterials.h"

#include "graphics/render/material/MaterialDocumentStore.h"
#include "graphics/render/material/MaterialManifestCatalog.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/ShaderManager.h"

namespace render
{
    StudioModelMaterials::StudioModelMaterials(
        MaterialDocumentStore& documents,
        MaterialManifestCatalog& manifests,
        const ShaderManager& shaders,
        const ServerMaterials& server_materials
    ) :
        documents_(documents),
        manifests_(manifests),
        shaders_(shaders),
        server_materials_(server_materials)
    {}

    void StudioModelMaterials::LoadDocument(const std::string& name)
    {
        document_loaded_ = true;
        document_ = documents_.LoadModel(name);
    }

    void StudioModelMaterials::Refresh(const std::string& name)
    {
        // Document validation and manifest defaults depend on the shader generation.
        if (shader_generation_ != shaders_.get_generation())
        {
            shader_generation_ = shaders_.get_generation();
            document_loaded_ = false;
            materials_.clear();
        }

        if (!document_loaded_)
        {
            document_ = ncl_material::MaterialDoc{};
            LoadDocument(name);
        }

        if (server_generation_ != server_materials_.get_generation())
        {
            server_generation_ = server_materials_.get_generation();
            materials_.clear();
        }

        if (materials_.empty())
        {
            origins_.clear();
            ncl_material::MaterialSources sources;
            sources.model = &document_;

            for (const ncl_studio::TextureInfo& texture : textures_->textures)
            {
                ncl_material::ResolvedMaterial resolved = ncl_material::Resolve(sources, texture.name, texture.flags);

                ncl_material::Material material = std::move(resolved.material);
                ncl_material::MaterialOrigin origin = resolved.origin;

                // Checked here, once per model, rather than at draw time: a material that does
                // not match its program would otherwise show up as a uniform that quietly did
                // nothing, which is the hardest kind of content error to chase.
                if (material.program != ncl_material::kLegacyProgram)
                {
                    const ncl_material::ProgramManifest* manifest = manifests_.Get(material.program);

                    // A program without a manifest can be neither checked nor have its samplers
                    // enumerated, so a material naming one would reach the draw path unvalidated
                    // and with its texture units handed out by position. The server table
                    // already refuses such an entry; a document beside a model is installed
                    // content just the same, and is refused on the same ground.
                    std::string reason;

                    if (manifest == nullptr)
                    {
                        reason = "no program \"" + material.program + "\"";
                    }
                    else
                    {
                        ncl_utils::Result<> valid = ncl_material::Validate(material, *manifest);

                        if (valid.has_error())
                        {
                            reason = valid.error_str();
                        }
                    }

                    if (reason.empty())
                    {
                        ncl_material::ApplyDefaults(material, *manifest);
                    }
                    else
                    {
                        // Back to the implicit material rather than to the engine path: the
                        // model still renders correctly, and the message says what to fix.
                        documents_.Report("render: %s, texture %s: %s\n", name.c_str(), texture.name.c_str(), reason.c_str());

                        material = ncl_material::MakeImplicitMaterial(texture.flags);
                        origin = ncl_material::MaterialOrigin::Implicit;
                    }
                }

                materials_.push_back(std::move(material));
                origins_.push_back(origin);
            }
        }
    }

    bool StudioModelMaterials::Prepare(
        const goldsrc::studiohdr_t* header,
        size_t size,
        const goldsrc::studiohdr_t* textures,
        size_t texture_size,
        std::string_view texture_owner
    )
    {
        if (metadata_attempted_ && texture_header_ == textures && texture_owner_ == texture_owner)
        {
            return metadata_.has_value() && textures_.has_value();
        }

        ncl_utils::ResultT<ncl_studio::StudioMetadata> metadata = ncl_studio::ReadStudioMetadata(header, size);
        ncl_utils::ResultT<ncl_studio::StudioTextures> texture_data = ncl_studio::ReadStudioTextures(textures, texture_size);
        metadata_attempted_ = true;
        texture_header_ = textures;
        texture_owner_ = texture_owner;
        metadata_.reset();
        textures_.reset();
        materials_.clear();
        origins_.clear();

        if (metadata.has_error() || texture_data.has_error())
        {
            return false;
        }

        metadata_ = *metadata;
        textures_ = *texture_data;
        return true;
    }

    bool StudioModelMaterials::is_ready() const
    {
        return metadata_.has_value() && textures_.has_value();
    }

    bool StudioModelMaterials::ForgetTextureOwner(const char* name)
    {
        if (texture_owner_ != name)
        {
            return false;
        }

        texture_header_ = nullptr;
        texture_owner_.clear();
        textures_.reset();
        metadata_attempted_ = false;
        materials_.clear();
        origins_.clear();
        return true;
    }

    const ncl_studio::StudioMetadata* StudioModelMaterials::get_metadata() const
    {
        return metadata_ ? &*metadata_ : nullptr;
    }

    const ncl_studio::StudioTextures* StudioModelMaterials::get_textures() const
    {
        return textures_ ? &*textures_ : nullptr;
    }

    const ncl_material::Material* StudioModelMaterials::Resolve(
        const std::string& name,
        int texture_index,
        ncl_material::MaterialOrigin* origin
    )
    {
        if (!metadata_ || !textures_ || texture_index < 0 || texture_index >= static_cast<int>(textures_->textures.size()))
        {
            return nullptr;
        }

        Refresh(name);

        if (origin != nullptr)
        {
            *origin = origins_[texture_index];
        }

        return &materials_[texture_index];
    }

    void StudioModelMaterials::ReloadDocument()
    {
        document_ = {};
        document_loaded_ = false;
        materials_.clear();
        origins_.clear();
    }

    void StudioModelMaterials::Forget()
    {
        metadata_.reset();
        textures_.reset();
        metadata_attempted_ = false;
        texture_header_ = nullptr;
        texture_owner_.clear();
        ReloadDocument();
    }
} // namespace render
