#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ncl_material/MaterialResolve.h>
#include <ncl_studio/StudioMetadata.h>
#include <ncl_studio/StudioTextures.h>

#include <goldsrc/fwd.h>

namespace render
{
    class MaterialDocumentStore;
    class MaterialManifestCatalog;
    class ShaderManager;
    class ServerMaterials;

    class StudioModelMaterials
    {
    private:
        MaterialDocumentStore& documents_;
        MaterialManifestCatalog& manifests_;
        const ShaderManager& shaders_;
        const ServerMaterials& server_materials_;
        std::optional<ncl_studio::StudioMetadata> metadata_;
        std::optional<ncl_studio::StudioTextures> textures_;
        bool metadata_attempted_{};
        const goldsrc::studiohdr_t* texture_header_{};
        std::string texture_owner_;
        ncl_material::MaterialDoc document_;
        bool document_loaded_{};
        unsigned int server_generation_{};
        uint32_t shader_generation_{};
        std::vector<ncl_material::Material> materials_;
        std::vector<ncl_material::MaterialOrigin> origins_;

    public:
        StudioModelMaterials(
            MaterialDocumentStore& documents,
            MaterialManifestCatalog& manifests,
            const ShaderManager& shaders,
            const ServerMaterials& server_materials
        );
        bool Prepare(
            const goldsrc::studiohdr_t* header,
            size_t size,
            const goldsrc::studiohdr_t* textures,
            size_t texture_size,
            std::string_view texture_owner
        );
        bool is_ready() const;
        bool ForgetTextureOwner(const char* name);
        const ncl_studio::StudioMetadata* get_metadata() const;
        const ncl_studio::StudioTextures* get_textures() const;
        const ncl_material::Material* Resolve(const std::string& name, int texture_index, ncl_material::MaterialOrigin* origin = nullptr);
        void ReloadDocument();
        void Forget();

    private:
        void LoadDocument(const std::string& name);
        void Refresh(const std::string& name);
    };
} // namespace render
