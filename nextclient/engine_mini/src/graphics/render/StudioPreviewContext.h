#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/render/StudioDraw.h"

namespace render
{
    namespace preview
    {
        class PreviewInteraction;
    }

    struct StudioPreviewInputs
    {
        const goldsrc::cl_entity_t* entity{};
        PreviewCamera camera{};
        PreviewLight light{};
        float view_right[3]{};
        float scale = 1.0f;
        double time{};
        uintptr_t draw_identity{};
        bool authoring{};
        bool materials_enabled = true;
        bool rejected{};
        preview::PreviewInteraction* capture{};
    };

    class StudioPreviewContext
    {
    private:
        StudioRenderer& renderer_;
        StudioPreviewInputs inputs_;
        StudioPreviewInputs* previous_;
        uint64_t cancellation_epoch_{};
        StudioQueuePause pause_;

    public:
        StudioPreviewContext(
            StudioRenderer& renderer,
            const goldsrc::cl_entity_t* entity,
            const PreviewDrawParams& params,
            const PreviewModel& model,
            preview::PreviewInteraction* capture = nullptr
        );
        ~StudioPreviewContext();

        StudioPreviewContext(const StudioPreviewContext&) = delete;
        StudioPreviewContext& operator=(const StudioPreviewContext&) = delete;

        bool is_rejected() const;
    };

} // namespace render
