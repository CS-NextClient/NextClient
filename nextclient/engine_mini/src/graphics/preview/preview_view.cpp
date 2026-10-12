#include "graphics/preview/preview_view.h"

#include <cmath>

#include <ncl_math/vec3.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include "engine.h"
#include "graphics/gl_local.h"

namespace
{
    // world units
    constexpr float kNearPlane = 4.0f;
    constexpr float kFarPlane = 4096.0f;

    void LoadFrustum(float half_width, float half_height)
    {
        qglMatrixMode(GL_PROJECTION);
        qglLoadIdentity();
        qglFrustum(-half_width, half_width, -half_height, half_height, kNearPlane, kFarPlane);
    }

    // The engine's view basis: GL looks down -Z, the engine down +X with Z up.
    void LoadBasis()
    {
        qglMatrixMode(GL_MODELVIEW);
        qglLoadIdentity();
        qglRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        qglRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    }
} // namespace

void PreviewView_LoadCamera(const PreviewCamera& camera, int wide, int tall)
{
    float half_width = std::tan(camera.fov * 0.5f * ncl_math::kDeg2Rad) * kNearPlane;
    float half_height = half_width * tall / wide;
    LoadFrustum(half_width, half_height);

    LoadBasis();
    qglRotatef(-camera.angles[2], 1.0f, 0.0f, 0.0f);
    qglRotatef(-camera.angles[0], 0.0f, 1.0f, 0.0f);
    qglRotatef(-camera.angles[1], 0.0f, 0.0f, 1.0f);
    qglTranslatef(-camera.origin[0], -camera.origin[1], -camera.origin[2]);
}

void PreviewView_LoadCameraSpace(float fov_vertical, bool mirror, int wide, int tall)
{
    float half_height = std::tan(fov_vertical * 0.5f * ncl_math::kDeg2Rad) * kNearPlane;
    float half_width = half_height * wide / tall;
    LoadFrustum(half_width, half_height);

    LoadBasis();

    if (mirror)
    {
        qglScalef(1.0f, -1.0f, 1.0f);
    }
}
