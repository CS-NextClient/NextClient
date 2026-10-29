#pragma once

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/render/RenderDevice.h"

namespace render::preview
{
    class PreviewSceneTarget : public render::IDeviceResource
    {
    private:
        friend class ScenePreviewTestAccess;
        RenderDevice& device_;
        unsigned int framebuffer_{};
        unsigned int color_{};
        unsigned int depth_{};
        unsigned int color_format_{};
        unsigned int depth_format_{};
        int samples_{};
        int wide_{};
        int tall_{};
        int caller_read_{};
        int caller_draw_{};
        int caller_renderbuffer_{};
        int caller_viewport_[4]{};
        int caller_scissor_[4]{};
        bool caller_scissor_enabled_{};
        bool caller_srgb_{};
        bool active_{};

    public:
        explicit PreviewSceneTarget(RenderDevice& device);
        ~PreviewSceneTarget() override;
        PreviewSceneTarget(const PreviewSceneTarget&) = delete;
        PreviewSceneTarget& operator=(const PreviewSceneTarget&) = delete;
        bool Begin(const PreviewRect& view, bool preserve_color);
        void Present(const PreviewRect& view, const PreviewRect& clip);
        void Cancel();
        void Clear();
        void OnDeviceLost() override;

    private:
        void Restore();
    };

} // namespace render::preview
