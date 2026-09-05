#include "graphics/preview/ScenePreview.h"

#include <memory>
#include <utility>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "engine.h"
#include "graphics/gl_local.h"
#include "graphics/preview/preview_model.h"
#include "graphics/preview/preview_view.h"
#include "graphics/preview/WavefrontScene.h"

namespace
{
    // Screen coordinates to frame buffer pixels: the two differ under -stretchaspect and in
    // full screen, and GL counts y from the bottom.
    PreviewRect ToFrameBuffer(const PreviewRect& rect, const SCREENINFO& screen, const GLint* viewport)
    {
        float scale_x = static_cast<float>(viewport[2]) / screen.iWidth;
        float scale_y = static_cast<float>(viewport[3]) / screen.iHeight;

        PreviewRect out;
        out.x = viewport[0] + static_cast<int>(rect.x * scale_x);
        out.y = viewport[1] + static_cast<int>((screen.iHeight - rect.y - rect.tall) * scale_y);
        out.wide = static_cast<int>(rect.wide * scale_x);
        out.tall = static_cast<int>(rect.tall * scale_y);

        return out;
    }

    void SetEnabled(GLenum capability, GLboolean enabled)
    {
        if (enabled)
        {
            qglEnable(capability);
        }
        else
        {
            qglDisable(capability);
        }
    }
} // namespace

PreviewSceneHandle ScenePreview::LoadScene(const char* path, const char* const* wads, int wad_count)
{
    std::unique_ptr<WavefrontScene> scene = std::make_unique<WavefrontScene>();
    PreviewSceneHandle handle = next_handle_;

    if (!scene->Load(path, wads, wad_count, handle))
    {
        return 0;
    }

    next_handle_++;
    scenes_.push_back(Entry{handle, std::move(scene)});

    return handle;
}

void ScenePreview::FreeScene(PreviewSceneHandle scene)
{
    for (size_t i = 0; i < scenes_.size(); i++)
    {
        if (scenes_[i].handle != scene)
        {
            continue;
        }

        scenes_.erase(scenes_.begin() + i);
        return;
    }
}

void ScenePreview::Draw(const PreviewDrawParams& params, const PreviewRect& rect, const PreviewRect& clip)
{
    SCREENINFO screen;
    screen.iSize = sizeof(screen);
    gEngfuncs.pfnGetScreenInfo(&screen);

    if (screen.iWidth <= 0 || screen.iHeight <= 0)
    {
        return;
    }

    // the viewport in force is the whole frame buffer, set by the engine once a frame
    GLint saved_viewport[4];
    qglGetIntegerv(GL_VIEWPORT, saved_viewport);

    PreviewRect view = ToFrameBuffer(rect, screen, saved_viewport);
    PreviewRect scissor = ToFrameBuffer(clip, screen, saved_viewport);

    if (view.wide <= 0 || view.tall <= 0 || scissor.wide <= 0 || scissor.tall <= 0)
    {
        return;
    }

    // the scene and the models load projections of their own
    qglMatrixMode(GL_PROJECTION);
    qglPushMatrix();
    qglMatrixMode(GL_MODELVIEW);
    qglPushMatrix();

    GLboolean saved_blend = qglIsEnabled(GL_BLEND);
    GLboolean saved_alpha_test = qglIsEnabled(GL_ALPHA_TEST);
    GLboolean saved_texture = qglIsEnabled(GL_TEXTURE_2D);
    GLboolean saved_depth_test = qglIsEnabled(GL_DEPTH_TEST);
    GLboolean saved_cull_face = qglIsEnabled(GL_CULL_FACE);
    GLboolean saved_scissor_test = qglIsEnabled(GL_SCISSOR_TEST);
    GLboolean saved_depth_mask = GL_TRUE;
    qglGetBooleanv(GL_DEPTH_WRITEMASK, &saved_depth_mask);

    GLint saved_scissor_box[4];
    qglGetIntegerv(GL_SCISSOR_BOX, saved_scissor_box);

    qglViewport(view.x, view.y, view.wide, view.tall);
    qglScissor(scissor.x, scissor.y, scissor.wide, scissor.tall);
    qglEnable(GL_SCISSOR_TEST);

    // glClear honours the depth write mask, which may be off on entry
    qglDepthMask(GL_TRUE);

    if (params.clear)
    {
        qglClearColor(params.clear_color[0], params.clear_color[1], params.clear_color[2], 1.0f);
        qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else
    {
        qglClear(GL_DEPTH_BUFFER_BIT);
    }

    // The state the world pass would have left: SetupRenderer sets only the texture
    // environment and the shade model, so a model is otherwise tested away silently.
    qglEnable(GL_DEPTH_TEST);
    qglEnable(GL_TEXTURE_2D);
    qglDisable(GL_BLEND);
    qglDisable(GL_ALPHA_TEST);
    qglDisable(GL_CULL_FACE);
    qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    WavefrontScene* scene = Find(params.scene);
    if (scene != nullptr && !scene->is_empty())
    {
        PreviewView_LoadCamera(params.camera, view.wide, view.tall);
        scene->Draw(params.light);
    }

    for (int i = 0; i < params.model_count; i++)
    {
        if (!params.models[i].camera_space)
        {
            PreviewModel_Draw(params.models[i], params.light, params.camera, view.wide, view.tall);
        }
    }

    // Camera-space models are drawn through fields of view of their own, so their depths do
    // not compare with the scene's.
    bool depth_cleared = false;

    for (int i = 0; i < params.model_count; i++)
    {
        if (!params.models[i].camera_space)
        {
            continue;
        }

        if (!depth_cleared)
        {
            qglClear(GL_DEPTH_BUFFER_BIT);
            depth_cleared = true;
        }

        PreviewModel_Draw(params.models[i], params.light, params.camera, view.wide, view.tall);
    }

    qglMatrixMode(GL_PROJECTION);
    qglPopMatrix();
    qglMatrixMode(GL_MODELVIEW);
    qglPopMatrix();

    qglDepthMask(saved_depth_mask);
    qglViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
    qglScissor(saved_scissor_box[0], saved_scissor_box[1], saved_scissor_box[2], saved_scissor_box[3]);

    SetEnabled(GL_BLEND, saved_blend);
    SetEnabled(GL_ALPHA_TEST, saved_alpha_test);
    SetEnabled(GL_TEXTURE_2D, saved_texture);
    SetEnabled(GL_DEPTH_TEST, saved_depth_test);
    SetEnabled(GL_CULL_FACE, saved_cull_face);
    SetEnabled(GL_SCISSOR_TEST, saved_scissor_test);
}

WavefrontScene* ScenePreview::Find(PreviewSceneHandle handle) const
{
    for (const Entry& entry : scenes_)
    {
        if (entry.handle == handle)
        {
            return entry.scene.get();
        }
    }

    return nullptr;
}

void ScenePreview::ReloadAll()
{
    for (const Entry& entry : scenes_)
    {
        entry.scene->Reload();
    }
}

void ScenePreview::FreeAll()
{
    scenes_.clear();
}
