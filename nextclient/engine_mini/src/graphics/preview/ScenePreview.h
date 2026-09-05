#pragma once

#include <memory>
#include <vector>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/preview/WavefrontScene.h"

class ScenePreview : public ScenePreviewInterface
{
    struct Entry
    {
        PreviewSceneHandle handle;
        std::unique_ptr<WavefrontScene> scene;
    };

private:
    std::vector<Entry> scenes_;
    PreviewSceneHandle next_handle_ = 1;

public:
    PreviewSceneHandle LoadScene(const char* path, const char* const* wads, int wad_count) override;
    void FreeScene(PreviewSceneHandle scene) override;
    void Draw(const PreviewDrawParams& params, const PreviewRect& rect, const PreviewRect& clip) override;
    void ReloadAll();
    void FreeAll();

private:
    WavefrontScene* Find(PreviewSceneHandle handle) const;
};
