#pragma once

#include <cstdint>

namespace render
{
    class RenderDevice;
    class ShaderManager;
    class MaterialTextureCache;
    class StudioModelCache;
    class MaterialEditor;
    class StudioRenderer;
    class StudioInterceptState;
    class StudioEffects;
    class TestQuad;
    class RenderCaptureSession;
    namespace preview
    {
        class ScenePreview;
    }

    class RendererLifecycleBackend
    {
    public:
        virtual ~RendererLifecycleBackend() = default;
        virtual void MapChanged() = 0;
        virtual void ReleaseContext() = 0;
        virtual void AbandonContext() = 0;
        virtual void Shutdown() = 0;
        virtual void UninstallTextSurface() = 0;
        virtual void ReleaseText() = 0;
        virtual void ForgetText() = 0;
    };

    RendererLifecycleBackend& GetRendererLifecycleBackend();

    class RendererLifecycle
    {
    private:
        RenderDevice& device_;
        ShaderManager& shaders_;
        MaterialTextureCache& textures_;
        StudioModelCache& models_;
        MaterialEditor& editor_;
        RenderCaptureSession& capture_;
        StudioRenderer& studio_;
        StudioInterceptState& intercept_;
        StudioEffects& effects_;
        preview::ScenePreview& preview_;
        TestQuad& test_quad_;
        RendererLifecycleBackend& external_;
        uint32_t handled_generation_{};
        bool context_handled_{};
        bool context_transition_{};
        bool terminal_{};
        bool fatal_complete_{};
        bool fatal_requested_{};
        bool terminal_owners_cleared_{};

    public:
        RendererLifecycle(
            RenderDevice& device,
            ShaderManager& shaders,
            MaterialTextureCache& textures,
            StudioModelCache& models,
            MaterialEditor& editor,
            RenderCaptureSession& capture,
            StudioRenderer& studio,
            StudioInterceptState& intercept,
            StudioEffects& effects,
            preview::ScenePreview& preview,
            TestQuad& test_quad,
            RendererLifecycleBackend& external
        );
        RendererLifecycle(const RendererLifecycle&) = delete;
        RendererLifecycle& operator=(const RendererLifecycle&) = delete;
        void OnMapReset();
        void OnDisconnectBegin();
        void OnClientStateClearing();
        void BeginShutdown();
        void ReleaseLiveContext();
        void OnContextLost();
        void OnContextCreated();
        void OnFatalShutdown();

    private:
        void ClearTerminalOwners();
        void FinishTerminalPhase();
    };

    RendererLifecycle& GetLifecycle();
} // namespace render
