#pragma once

#include <next_engine_mini/ScenePreviewInterface.h>

// Declared rather than included: r_studioint.h names vec_t and byte without declaring them,
// so it compiles only after the engine's own headers.
typedef struct engine_studio_api_s engine_studio_api_t;

void PreviewModel_PatchStudioApi(engine_studio_api_t* api);
void PreviewModel_RestoreStudioApi();
// Returns what StudioDrawModel answered, or -1 when there was nothing to draw with.
int PreviewModel_Draw(const PreviewModel& model, const PreviewLight& light, const PreviewCamera& camera, int wide, int tall);
