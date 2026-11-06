#include "graphics/preview/ScenePreview.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <glad/glad.h>
#include <ncl_utils/scope_exit.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include <goldsrc/engine/client_state.h>

#include "graphics/preview/PreviewModelRepository.h"
#include "graphics/gl_local.h"
#include "graphics/preview/preview_model.h"
#include "graphics/render/ScopedGlState.h"
#include "graphics/render/StudioBindings.h"
#include "graphics/render/effects/studio_effects.h"
#include "graphics/preview/preview_sun_gizmo.h"
#include "graphics/render/StudioRenderer.h"
#include "graphics/render/material/material_textures.h"
#include "graphics/render/model/studio_model_cache.h"
#include "graphics/preview/preview_view.h"
#include "graphics/preview/WavefrontScene.h"
#include "graphics/render/render_device.h"

namespace render::preview
{
    namespace
    {
        // Screen coordinates to frame buffer pixels: the two differ under -stretchaspect and in
        // full screen, and GL counts y from the bottom.
        PreviewRect ToFrameBuffer(const PreviewRect& rect, const goldsrc::SCREENINFO& screen, const GLint* viewport)
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

    } // namespace

    ScenePreview::ScenePreview(
        RenderDevice& device,
        ShaderManager& shaders,
        StudioModelCache& models,
        MaterialTextureCache& textures,
        PreviewModelRenderer& model_renderer,
        PreviewModelRepository& model_repository,
        PreviewEngineBindings& engine,
        StudioEffects& effects,
        MaterialCoverageRuntime& coverage,
        PreviewAssetBackend& assets,
        StudioRenderer& studio
    ) :
        device_(device),
        studio_(studio),
        models_(models),
        textures_(textures),
        model_renderer_(model_renderer),
        model_repository_(model_repository),
        engine_(engine),
        effects_(effects),
        assets_(assets),
        interaction_(device, shaders, coverage),
        target_(device)
    {}

    bool ScenePreview::is_authoring_available() const
    {
        return engine_.client != nullptr && engine_.client->state == goldsrc::ca_disconnected;
    }

    PreviewSceneHandle ScenePreview::LoadScene(const char* path, const char* const* wads, int wad_count)
    {
        if (!assets_.is_available())
        {
            return 0;
        }

        std::unique_ptr<WavefrontScene> scene = std::make_unique<WavefrontScene>(device_, assets_);
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

            scenes_[i].scene->Clear();
            scenes_.erase(scenes_.begin() + i);
            return;
        }
    }

    PreviewModelStatus ScenePreview::PrepareModel(const char* path, PreviewModelInfo* out)
    {
        return model_repository_.PrepareModel(path, out, nullptr, studio_.get_resource_barrier());
    }

    bool ScenePreview::GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out)
    {
        return model_repository_.GetSequenceInfo(path, index, out);
    }

    PreviewDrawStatus ScenePreview::Draw(const PreviewDrawParams& params, const PreviewRect& rect, const PreviewRect& clip, PreviewHit* hit)
    {
        if (hit != nullptr)
        {
            *hit = PreviewHit{};
        }

        interaction_.BeginFrame();
        goldsrc::SCREENINFO screen{};
        screen.iSize = sizeof(screen);
        if (engine_.screen_info != nullptr)
        {
            engine_.screen_info(&screen);
        }

        if (screen.iWidth <= 0 || screen.iHeight <= 0)
        {
            return PreviewDrawStatus::InvalidViewport;
        }

        GLint viewport[4];
        qglGetIntegerv(GL_VIEWPORT, viewport);
        PreviewRect view = ToFrameBuffer(rect, screen, viewport);
        PreviewRect scissor = ToFrameBuffer(clip, screen, viewport);
        int right = std::min(view.x + view.wide, scissor.x + scissor.wide);
        int top = std::min(view.y + view.tall, scissor.y + scissor.tall);
        scissor.x = std::max(view.x, scissor.x);
        scissor.y = std::max(view.y, scissor.y);
        scissor.wide = right - scissor.x;
        scissor.tall = top - scissor.y;

        if (view.wide <= 0 || view.tall <= 0 || scissor.wide <= 0 || scissor.tall <= 0)
        {
            return PreviewDrawStatus::InvalidViewport;
        }

        if (params.mode == PreviewRenderMode::MaterialAuthoring && !device_.CheckAvailable())
        {
            return PreviewDrawStatus::RendererUnavailable;
        }

        const uint64_t entry_epoch = studio_.get_cancellation_epoch();
        const uint32_t entry_generation = device_.get_generation();
        studio_.Flush(render::StudioFlushReason::Client);
        if (entry_epoch != studio_.get_cancellation_epoch() || entry_generation != device_.get_generation())
        {
            return PreviewDrawStatus::RendererUnavailable;
        }
        render::ScopedGlState saved_state(device_, &studio_.get_cancellation_source());
        render::StudioQueuePause pause(studio_);
        auto target_scope = ncl_utils::MakeScopeExit([this] { target_.Cancel(); });
        const PreviewRect output_view = view;
        const PreviewRect output_clip = scissor;
        if (params.mode == PreviewRenderMode::MaterialAuthoring)
        {
            if (!target_.Begin(view, !params.clear))
            {
                return PreviewDrawStatus::RendererUnavailable;
            }
            scissor.x -= view.x;
            scissor.y -= view.y;
            view.x = 0;
            view.y = 0;
        }
        qglViewport(view.x, view.y, view.wide, view.tall);
        qglScissor(scissor.x, scissor.y, scissor.wide, scissor.tall);
        qglEnable(GL_SCISSOR_TEST);
        qglDepthMask(GL_TRUE);
        qglDepthFunc(GL_LEQUAL);
        qglClearDepth(1.0);
        qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        if (params.clear)
        {
            qglClearColor(params.clear_color[0], params.clear_color[1], params.clear_color[2], 1.0f);
            qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        else
        {
            qglClear(GL_DEPTH_BUFFER_BIT);
        }

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

        PreviewDrawStatus result = PreviewDrawStatus::Ready;

        for (int pass = 0; pass < 2; pass++)
        {
            const uint64_t view_identity = effects_.BeginView();
            const uint64_t view_epoch = studio_.get_cancellation_epoch();
            bool depth_cleared = false;

            for (int index = 0; index < params.model_count; index++)
            {
                if (params.models[index].camera_space != (pass == 1))
                {
                    continue;
                }

                // Camera-space models use a separate projection and cannot share scene depth.
                if (pass == 1 && !depth_cleared)
                {
                    qglClear(GL_DEPTH_BUFFER_BIT);
                    depth_cleared = true;
                }

                bool interactive = params.interactive && params.mode == PreviewRenderMode::MaterialAuthoring;
                interaction_.BeginModel(index, params.models[index]);

                const uint64_t epoch = studio_.get_cancellation_epoch();
                if (model_renderer_.Draw(params.models[index], params, view.wide, view.tall, interactive ? &interaction_ : nullptr) < 0)
                {
                    result = PreviewDrawStatus::ModelDrawFailed;
                }
                if (epoch != studio_.get_cancellation_epoch())
                {
                    effects_.CancelView(view_identity);
                    return PreviewDrawStatus::ModelDrawFailed;
                }
            }

            if (params.occluder && params.mode == PreviewRenderMode::MaterialAuthoring && (pass == 0 || depth_cleared))
            {
                render::ScopedGlState occluder_state;
                glUseProgram(0);
                for (int unit = 0; unit < render::kStudioTouchedTextureUnits; unit++)
                {
                    glActiveTexture(GL_TEXTURE0 + unit);
                    glDisable(GL_TEXTURE_2D);
                }
                glActiveTexture(GL_TEXTURE0);
                glDisable(GL_LIGHTING);
                glDisable(GL_FOG);
                glDisable(GL_CULL_FACE);
                glDisable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                glDisable(GL_STENCIL_TEST);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_ALWAYS);
                glDepthMask(GL_TRUE);
                glDepthRange(0, 1);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glColor4f(.24f, .26f, .29f, 1);
                glBegin(GL_QUADS);
                glVertex3f(-.85f, -.8f, -.999f);
                glVertex3f(0, -.8f, -.999f);
                glVertex3f(0, .8f, -.999f);
                glVertex3f(-.85f, .8f, -.999f);
                glEnd();
            }

            studio_.Flush(render::StudioFlushReason::Client);
            if (view_epoch != studio_.get_cancellation_epoch())
            {
                effects_.CancelView(view_identity);
                return PreviewDrawStatus::ModelDrawFailed;
            }
            effects_.EndView();
        }

        if (result == PreviewDrawStatus::Ready && params.interactive && params.mode == PreviewRenderMode::MaterialAuthoring)
        {
            int x = params.cursor_x < 0 ? -1 : static_cast<int>(static_cast<int64_t>(params.cursor_x) * view.wide / rect.wide);
            int y =
                params.cursor_y < 0 ? -1 : view.tall - 1 - static_cast<int>(static_cast<int64_t>(params.cursor_y) * view.tall / rect.tall);
            PreviewHit selected = interaction_.Render(view, scissor, x, y);

            if (hit != nullptr)
            {
                *hit = selected;
            }
        }

        if (params.sun_gizmo && params.mode == PreviewRenderMode::MaterialAuthoring)
        {
            PreviewSunGizmo_Draw(params.camera, params.light, view);
        }

        if (params.mode == PreviewRenderMode::MaterialAuthoring)
        {
            target_.Present(output_view, output_clip);
        }

        return result;
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

    void ScenePreview::CloseAuthoring()
    {
        if (device_.IsContextCurrent())
        {
            studio_.Flush(render::StudioFlushReason::Resource);
        }

        // Cached draw and pick packets borrow geometry names across calls.
        studio_.DiscardPending();
        interaction_.Clear();
        target_.Clear();
        models_.ReleaseGeometry();
        textures_.Clear();
    }

    void ScenePreview::ReloadAll()
    {
        if (!assets_.is_available())
        {
            return;
        }

        for (const Entry& entry : scenes_)
        {
            entry.scene->Reload();
        }
    }

    void ScenePreview::FreeAll()
    {
        interaction_.Clear();
        target_.Clear();
        for (const Entry& entry : scenes_)
        {
            entry.scene->Clear();
        }
        scenes_.clear();
    }

    void ScenePreview::DiscardCapturedWork()
    {
        interaction_.DiscardPackets();
    }

    void ScenePreview::RestoreStudioApi()
    {
        model_renderer_.RestoreStudioApi();
    }
} // namespace render::preview
