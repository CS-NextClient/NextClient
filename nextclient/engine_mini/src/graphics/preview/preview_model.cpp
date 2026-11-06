#include "graphics/preview/preview_model.h"

#include <goldsrc/entity.h>

#include "graphics/render/RendererState.h"

void PreviewModel_SetAnimationPose(goldsrc::cl_entity_t& entity, int sequence, int frame_count, float normalized_frame, float time)
{
    goldsrc::entity_state_t& state = entity.curstate;
    state.sequence = sequence;
    state.frame = normalized_frame * 256.0f;
    state.animtime = time;
    state.framerate = 0.0f;
    entity.latched.prevanimtime = state.animtime;
    entity.latched.prevsequence = state.sequence;
    entity.latched.prevframe = frame_count > 1 ? normalized_frame * static_cast<float>(frame_count - 1) : 0.0f;
    // Preview owns the complete pose; the stock transition blends against the client clock.
    entity.latched.sequencetime = 0.0f;
}

void PreviewModel_PatchStudioApi(goldsrc::engine_studio_api_t* api)
{
    render::GetRendererState().preview_renderer.PatchStudioApi(api);
}

void PreviewModel_RestoreStudioApi()
{
    render::GetRendererState().preview_renderer.RestoreStudioApi();
}

void PreviewModel_BindStudioInterface(goldsrc::r_studio_interface_t* studio)
{
    render::GetRendererState().preview_engine.studio = studio;
}

int PreviewModel_Draw(
    const PreviewModel& model,
    const PreviewDrawParams& params,
    int wide,
    int tall,
    render::preview::PreviewInteraction* capture
)
{
    return render::GetRendererState().preview_renderer.Draw(model, params, wide, tall, capture);
}
