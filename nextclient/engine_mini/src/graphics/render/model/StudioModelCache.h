#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/model/StudioModelMaterials.h"
#include "graphics/render/render_device.h"

namespace render
{
    class MaterialDocumentStore;
    class MaterialManifestCatalog;
    class ShaderManager;
    class ServerMaterials;

    struct StudioModelBarrier
    {
        void* user{};
        void (*before_replacement)(void*){};
    };

    class StudioModelCache : public IDeviceResource
    {
    private:
        struct Entry
        {
            std::string name;
            const goldsrc::studiohdr_t* header{};
            uint64_t session{};
            StudioGeometry geometry;
            StudioModelMaterials materials;

            Entry(
                RenderDevice& device,
                MaterialDocumentStore& documents,
                MaterialManifestCatalog& manifests,
                const ShaderManager& shaders,
                const ServerMaterials& server_materials,
                performance::PerformanceMonitor& monitor
            ) :
                geometry(device, monitor),
                materials(documents, manifests, shaders, server_materials)
            {}
        };


        performance::PerformanceMonitor& monitor_;
        RenderDevice& device_;
        MaterialDocumentStore& documents_;
        MaterialManifestCatalog& manifests_;
        const ShaderManager& shaders_;
        const ServerMaterials& server_materials_;
        std::vector<std::unique_ptr<Entry>> models_;
        std::vector<std::string> sorted_names_;
        Entry* last_entry_{};
        uint64_t session_ = 1;
        uint64_t mutation_epoch_{};

    public:
        StudioModelCache(
            RenderDevice& device,
            MaterialDocumentStore& documents,
            MaterialManifestCatalog& manifests,
            const ShaderManager& shaders,
            const ServerMaterials& server_materials,
            performance::PerformanceMonitor& monitor
        );
        ~StudioModelCache() override;
        StudioModelCache(const StudioModelCache&) = delete;
        StudioModelCache& operator=(const StudioModelCache&) = delete;
        void OnDeviceLost() override;
        bool PrepareMetadata(
            goldsrc::model_t* model,
            const goldsrc::studiohdr_t* header,
            const goldsrc::studiohdr_t* texture_header,
            const goldsrc::model_t* texture_model,
            StudioModelBarrier barrier = {}
        );
        const StudioGpuModel* PrepareGeometry(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, StudioModelBarrier barrier = {});
        const ncl_studio::StudioMetadata* Metadata(const char* model);
        const ncl_material::Material* ResolveMaterial(
            goldsrc::model_t* model,
            const goldsrc::studiohdr_t* header,
            const goldsrc::studiohdr_t* texture_header,
            int texture_index,
            const goldsrc::model_t* texture_model,
            StudioModelBarrier barrier = {},
            ncl_material::MaterialOrigin* origin = nullptr
        );
        const ncl_material::Material* ResolveMaterialByName(
            const char* model,
            const char* texture_name,
            ncl_material::MaterialOrigin* origin
        );
        int ModelCount();
        const char* ModelName(int index);
        int TextureCount(const char* model);
        const char* TextureName(const char* model, int index);
        int SkinFamilyCount(const char* model);
        int BodyCount(const char* model);
        bool BodyName(const char* model, int body, char* out, int out_len);
        void ReloadDocuments();
        // Retires one model and invalidates dependent texture readiness before owner reuse. No GL calls.
        void ForgetModel(const char* model);
        // Requires pending and queued studio references to have been discarded. No GL calls.
        void ForgetSessionMetadata();
        // Requires pending, queued, and preview-captured studio references to have been discarded. Preserves CPU identity and metadata.
        void ReleaseGeometry();
        // Requires pending and queued studio references to have been discarded.
        void Clear();

    private:
        Entry* Observe(goldsrc::model_t* model, const goldsrc::studiohdr_t* header, StudioModelBarrier barrier);
        Entry* FindMetadata(const char* name);
        Entry* FindByName(const char* name);
    };
} // namespace render
