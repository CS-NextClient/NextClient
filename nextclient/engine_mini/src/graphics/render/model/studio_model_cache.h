#pragma once

#include <ncl_material/MaterialResolve.h>
#include <ncl_studio/StudioMetadata.h>

#include <goldsrc/fwd.h>

namespace render
{
    struct StudioGpuModel;

    bool Models_PrepareMetadata(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        const goldsrc::studiohdr_t* texture_header,
        const goldsrc::model_t* texture_model
    );
    const StudioGpuModel* Models_PrepareGeometry(goldsrc::model_t* model, const goldsrc::studiohdr_t* header);
    const ncl_studio::StudioMetadata* Models_Metadata(const char* model);
    const ncl_material::Material* Models_ResolveMaterial(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        const goldsrc::studiohdr_t* texture_header,
        int texture_index,
        const goldsrc::model_t* texture_model
    );
    const ncl_material::Material* Models_ResolveMaterialByName(
        const char* model,
        const char* texture_name,
        ncl_material::MaterialOrigin* origin
    );
    int Models_ModelCount();
    const char* Models_ModelName(int index);
    int Models_TextureCount(const char* model);
    const char* Models_TextureName(const char* model, int index);
    int Models_SkinFamilyCount(const char* model);
    int Models_BodyCount(const char* model);
    bool Models_BodyName(const char* model, int body, char* out, int out_len);
    void Models_ReloadDocuments();
    // Retires one model and invalidates dependent texture readiness before owner reuse. No GL calls.
    void Models_ForgetModel(const char* model);
    // Requires pending and queued studio references to have been discarded. No GL calls.
    void Models_ForgetSessionMetadata();
    // Requires pending, queued, and preview-captured studio references to have been discarded. Preserves CPU identity and metadata.
    void Models_ReleaseGeometry();
    // Requires pending and queued studio references to have been discarded.
    void Models_Clear();
} // namespace render
