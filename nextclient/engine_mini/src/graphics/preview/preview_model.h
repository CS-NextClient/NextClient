#pragma once

#include <next_engine_mini/ScenePreviewInterface.h>

#include <goldsrc/fwd.h>

void PreviewModel_PatchStudioApi(goldsrc::engine_studio_api_t* api);
void PreviewModel_RestoreStudioApi();
// Returns what StudioDrawModel answered, or -1 when there was nothing to draw with.
int PreviewModel_Draw(const PreviewModel& model, const PreviewLight& light, const PreviewCamera& camera, int wide, int tall);
