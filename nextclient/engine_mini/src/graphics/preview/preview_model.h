#pragma once

#include <next_engine_mini/ScenePreviewInterface.h>

#include <goldsrc/fwd.h>

namespace render::preview
{
    class PreviewInteraction;
}

void PreviewModel_PatchStudioApi(goldsrc::engine_studio_api_t* api);
void PreviewModel_RestoreStudioApi();
void PreviewModel_BindStudioInterface(goldsrc::r_studio_interface_t* studio);
void PreviewModel_SetAnimationPose(goldsrc::cl_entity_t& entity, int sequence, int frame_count, float normalized_frame, float time);
// Returns what StudioDrawModel answered, or -1 when there was nothing to draw with.
int PreviewModel_Draw(
    const PreviewModel& model,
    const PreviewDrawParams& params,
    int wide,
    int tall,
    render::preview::PreviewInteraction* capture = nullptr
);

PreviewModelStatus PreviewModel_Prepare(const char* path, PreviewModelInfo* out);
bool PreviewModel_GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out);
int PreviewModel_LoadedEntries();
const char* PreviewModel_LastLoadError();
void PreviewModel_ResetModels();
