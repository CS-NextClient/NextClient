#pragma once

#include <memory>
#include <vector>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/WavefrontScene.h"
#include "graphics/preview/PreviewInteraction.h"
#include "graphics/preview/PreviewSceneTarget.h"
#include "graphics/preview/PreviewModelRenderer.h"
#include "graphics/render/effects/StudioEffects.h"
#include "graphics/render/model/StudioModelCache.h"
#include "graphics/render/material/MaterialTextureCache.h"

namespace render::preview
{
    class ScenePreview : public ScenePreviewInterface
    {
        struct Entry
        {
            PreviewSceneHandle handle;
            std::unique_ptr<WavefrontScene> scene;
        };

    private:
        friend class ScenePreviewTestAccess;
        RenderDevice& device_;
        StudioRenderer& studio_;
        StudioModelCache& models_;
        MaterialTextureCache& textures_;
        PreviewModelRenderer& model_renderer_;
        PreviewModelRepository& model_repository_;
        PreviewEngineBindings& engine_;
        StudioEffects& effects_;
        PreviewAssetBackend& assets_;
        std::vector<Entry> scenes_;
        PreviewSceneHandle next_handle_ = 1;
        PreviewInteraction interaction_;
        PreviewSceneTarget target_;

    public:
        ScenePreview(
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
        );
        ~ScenePreview() override = default;
        ScenePreview(const ScenePreview&) = delete;
        ScenePreview& operator=(const ScenePreview&) = delete;
        bool is_authoring_available() const override;
        PreviewSceneHandle LoadScene(const char* path, const char* const* wads, int wad_count) override;
        void FreeScene(PreviewSceneHandle scene) override;
        PreviewModelStatus PrepareModel(const char* path, PreviewModelInfo* out) override;
        bool GetSequenceInfo(const char* path, int index, PreviewSequenceInfo* out) override;
        PreviewDrawStatus Draw(
            const PreviewDrawParams& params,
            const PreviewRect& rect,
            const PreviewRect& clip,
            PreviewHit* hit = nullptr
        ) override;
        void CloseAuthoring() override;
        void ReloadAll();
        void FreeAll();
        void RestoreStudioApi();
        void DiscardCapturedWork();

    private:
        WavefrontScene* Find(PreviewSceneHandle handle) const;
    };

} // namespace render::preview
