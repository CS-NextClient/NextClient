#pragma once

#include <goldsrc/entity.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/PreviewEngineBindings.h"

namespace render
{
    class StudioRenderer;
}

namespace render::preview
{
    class PreviewInteraction;
    class PreviewModelRepository;

    class PreviewModelRenderer
    {
    private:
        PreviewModelRepository& models_;
        PreviewEngineBindings& engine_;
        StudioRenderer& studio_;
        goldsrc::cl_entity_t entity_{};
        int ambient_light_{};
        int shade_light_{};
        float light_color_[3]{};
        float light_direction_[3]{};
        void (*original_dynamic_light_)(goldsrc::cl_entity_t*, goldsrc::alight_t*){};
        int (*original_check_bbox_)(){};
        goldsrc::engine_studio_api_t* patched_api_{};

    public:
        PreviewModelRenderer(PreviewModelRepository& models, PreviewEngineBindings& engine, StudioRenderer& studio);
        ~PreviewModelRenderer() = default;
        PreviewModelRenderer(const PreviewModelRenderer&) = delete;
        PreviewModelRenderer& operator=(const PreviewModelRenderer&) = delete;
        void PatchStudioApi(goldsrc::engine_studio_api_t* api);
        void RestoreStudioApi();
        int Draw(const PreviewModel& model, const PreviewDrawParams& params, int wide, int tall, PreviewInteraction* capture = nullptr);
        void DynamicLight(goldsrc::cl_entity_t* entity, goldsrc::alight_t* light);
        int CheckBBox();

    private:
        void SetupEntity(
            goldsrc::model_t* model,
            int sequence,
            int frame_count,
            float frame,
            float time,
            const float* origin,
            const float* angles
        );
    };
} // namespace render::preview
