#pragma once

#include <next_engine_mini/ScenePreviewInterface.h>

// The engine's own view basis, X forward and Z up, with the camera's angles turned back and
// its origin taken out.
void PreviewView_LoadCamera(const PreviewCamera& camera, int wide, int tall);

// A camera's own space: at the origin looking down +X, turned over about the lateral axis
// when mirror is set.
void PreviewView_LoadCameraSpace(float fov_vertical, bool mirror, int wide, int tall);
