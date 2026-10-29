#include "graphics/render/StudioPreviewContext.h"

#include <algorithm>

#include <ncl_math/vec3.h>

#include "graphics/preview/PreviewInteraction.h"
#include "graphics/render/StudioRenderer.h"

namespace render
{
    StudioPreviewContext::StudioPreviewContext(
        StudioRenderer& renderer,
        const goldsrc::cl_entity_t* entity,
        const PreviewDrawParams& params,
        const PreviewModel& model,
        preview::PreviewInteraction* capture
    ) :
        renderer_(renderer),
        previous_(renderer.preview_inputs_),
        cancellation_epoch_(renderer.get_cancellation_epoch()),
        pause_(renderer)
    {
        inputs_.entity = entity;
        inputs_.capture = params.mode == PreviewRenderMode::MaterialAuthoring ? capture : nullptr;
        inputs_.camera = params.camera;
        inputs_.light = params.light;
        inputs_.scale = model.scale;
        inputs_.time = params.time;
        inputs_.draw_identity = reinterpret_cast<uintptr_t>(&model);
        inputs_.authoring = params.mode == PreviewRenderMode::MaterialAuthoring;
        inputs_.materials_enabled = params.materials_enabled;

        float forward[3];
        float up[3];
        ncl_math::AngleVectors(params.camera.angles, forward, inputs_.view_right, up);

        if (model.camera_space)
        {
            inputs_.camera = PreviewCamera{};
            inputs_.camera.fov = model.fov;

            inputs_.light.direction[0] = ncl_math::DotProduct(params.light.direction, forward);
            inputs_.light.direction[1] = -ncl_math::DotProduct(params.light.direction, inputs_.view_right);
            inputs_.light.direction[2] = ncl_math::DotProduct(params.light.direction, up);

            inputs_.view_right[0] = 0.0f;
            inputs_.view_right[1] = model.mirror ? 1.0f : -1.0f;
            inputs_.view_right[2] = 0.0f;

            if (model.mirror)
            {
                inputs_.light.direction[1] = -inputs_.light.direction[1];
            }
        }

        renderer_.preview_inputs_ = &inputs_;
    }

    StudioPreviewContext::~StudioPreviewContext()
    {
        if (cancellation_epoch_ == renderer_.get_cancellation_epoch())
        {
            renderer_.Flush(StudioFlushReason::Client);
        }
        else if (previous_ != nullptr)
        {
            previous_->rejected = true;
            previous_->capture = nullptr;
        }
        renderer_.preview_inputs_ = previous_;
    }

    bool StudioPreviewContext::is_rejected() const
    {
        return inputs_.rejected || cancellation_epoch_ != renderer_.get_cancellation_epoch();
    }

} // namespace render
