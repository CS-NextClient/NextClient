#include "graphics/render/model/studio_model_cache.h"

#include "graphics/render/RendererState.h"
#include "graphics/render/StudioDraw.h"

namespace render
{
    namespace
    {
        void FlushReplacement(void*)
        {
            Studio_Flush(StudioFlushReason::Resource);
        }
    } // namespace

    bool Models_PrepareMetadata(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        const goldsrc::studiohdr_t* texture_header,
        const goldsrc::model_t* texture_model
    )
    {
        return GetRendererState().models.PrepareMetadata(model, header, texture_header, texture_model, {nullptr, FlushReplacement});
    }

    const StudioGpuModel* Models_PrepareGeometry(goldsrc::model_t* model, const goldsrc::studiohdr_t* header)
    {
        return GetRendererState().models.PrepareGeometry(model, header, {nullptr, FlushReplacement});
    }

    const ncl_studio::StudioMetadata* Models_Metadata(const char* model)
    {
        return GetRendererState().models.Metadata(model);
    }

    const ncl_material::Material* Models_ResolveMaterial(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        const goldsrc::studiohdr_t* texture_header,
        int texture_index,
        const goldsrc::model_t* texture_model
    )
    {
        return GetRendererState().models.ResolveMaterial(
            model, header, texture_header, texture_index, texture_model, {nullptr, FlushReplacement}
        );
    }

    const ncl_material::Material* Models_ResolveMaterialByName(
        const char* model,
        const char* texture_name,
        ncl_material::MaterialOrigin* origin
    )
    {
        return GetRendererState().models.ResolveMaterialByName(model, texture_name, origin);
    }

    int Models_ModelCount()
    {
        return GetRendererState().models.ModelCount();
    }

    const char* Models_ModelName(int index)
    {
        return GetRendererState().models.ModelName(index);
    }

    int Models_TextureCount(const char* model)
    {
        return GetRendererState().models.TextureCount(model);
    }

    const char* Models_TextureName(const char* model, int index)
    {
        return GetRendererState().models.TextureName(model, index);
    }

    int Models_SkinFamilyCount(const char* model)
    {
        return GetRendererState().models.SkinFamilyCount(model);
    }

    int Models_BodyCount(const char* model)
    {
        return GetRendererState().models.BodyCount(model);
    }

    bool Models_BodyName(const char* model, int body, char* out, int out_len)
    {
        return GetRendererState().models.BodyName(model, body, out, out_len);
    }

    void Models_ReloadDocuments()
    {
        GetRendererState().models.ReloadDocuments();
    }

    void Models_ForgetModel(const char* model)
    {
        GetRendererState().models.ForgetModel(model);
    }

    void Models_ForgetSessionMetadata()
    {
        GetRendererState().models.ForgetSessionMetadata();
    }

    void Models_ReleaseGeometry()
    {
        GetRendererState().models.ReleaseGeometry();
    }

    void Models_Clear()
    {
        GetRendererState().models.Clear();
    }

} // namespace render
