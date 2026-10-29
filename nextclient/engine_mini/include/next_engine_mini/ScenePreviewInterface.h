#pragma once

#include <tier1/interface.h>

enum class PreviewModelStatus
{
    Ready,
    NotFound,
    InvalidModel,
    CapacityExceeded,
    RendererUnavailable
};

enum class PreviewRenderMode
{
    Configured,
    MaterialAuthoring
};

enum class PreviewDrawStatus
{
    Ready,
    InvalidViewport,
    RendererUnavailable,
    ModelDrawFailed
};

struct PreviewModelInfo
{
    float bounds_min[3]{};
    float bounds_max[3]{};
    int skin_family_count{};
    int body_count{};
    int sequence_count{};
};

struct PreviewSequenceInfo
{
    char name[64]{};
    float duration{};
};
// Screen coordinates, y measured from the top.
struct PreviewRect
{
    int x;
    int y;
    int wide;
    int tall;
};

// World units and degrees on the engine's axes: X forward, Y to the left, Z up.
struct PreviewCamera
{
    float origin[3];
    float angles[3];    // pitch, yaw, roll
    float fov;          // horizontal, degrees
};

// direction is a unit vector along the travel of the light. A scene shades its textures by
// color * (ambient + diffuse * lambert); model_ambient and model_shade are the studio
// renderer's own terms, 0..255, which it clips at 128 and 255.
struct PreviewLight
{
    float direction[3];
    float color[3];
    float ambient;
    float diffuse;
    int model_ambient;
    int model_shade;
};

// A world model stands in the scene. A camera-space one is held against the camera like the
// engine's view model: origin and angles are in the camera's space, it is drawn in front of
// everything else, and mirror turns it over for the other hand.
struct PreviewModel
{
    const char* path;       // under the game directory, e.g. "models/v_ak47.mdl"
    int sequence;
    float frame;            // position inside the sequence, 0..1
    float origin[3];
    float angles[3];        // pitch, yaw, roll
    bool camera_space;
    float fov;              // horizontal, degrees; camera-space models only
    bool mirror;            // camera-space models only
    int skin{};
    int body{};
    float scale = 1.0f;
};

// A scene LoadScene answered with, or 0.
using PreviewSceneHandle = int;

struct PreviewHit
{
    int model_index = -1;
    int texture_index = -1;
};

struct PreviewDrawParams
{
    PreviewCamera camera;
    PreviewLight light;
    PreviewSceneHandle scene;       // 0 draws no scene

    const PreviewModel* models;
    int model_count;

    bool clear;
    float clear_color[3];
    double time{};
    PreviewRenderMode mode{};
    bool materials_enabled = true;
    bool interactive{};
    int cursor_x = -1; // local viewport coordinates, y from the top
    int cursor_y = -1;
    bool sun_gizmo{};
    bool occluder{};
};

class ScenePreviewInterface : public IBaseInterface
{
public:
    virtual bool is_authoring_available() const = 0;
    virtual PreviewSceneHandle LoadScene(const char* path, const char* const* wads, int wad_count) = 0;
    virtual void FreeScene(PreviewSceneHandle scene) = 0;
    virtual PreviewModelStatus PrepareModel(const char* path, PreviewModelInfo* out) = 0;
    virtual bool GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out) = 0;
    virtual PreviewDrawStatus Draw(
        const PreviewDrawParams& params,
        const PreviewRect& rect,
        const PreviewRect& clip,
        PreviewHit* hit = nullptr
    ) = 0;
    // Requires preview drawing to have finished. Preserves scenes and model-session identity.
    virtual void CloseAuthoring() = 0;
};

#define SCENE_PREVIEW_INTERFACE_VERSION "ScenePreview002"
