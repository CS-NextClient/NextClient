#include "graphics/render/RendererLifecycle.h"

#include "graphics/preview/ScenePreview.h"
#include "graphics/render/material/MaterialEditor.h"
#include "graphics/render/RenderCaptureSession.h"
#include "graphics/render/TestQuad.h"
#include "graphics/render/material/MaterialTextureCache.h"
#include "graphics/render/RenderDevice.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/StudioRenderer.h"
#include "graphics/render/effects/StudioEffects.h"
#include "graphics/render/StudioIntercept.h"
#include "graphics/render/model/StudioModelCache.h"
#include "graphics/render/render_context.h"

namespace render
{
    RendererLifecycle::RendererLifecycle(
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
    ) :
        device_(device),
        shaders_(shaders),
        textures_(textures),
        models_(models),
        editor_(editor),
        capture_(capture),
        studio_(studio),
        intercept_(intercept),
        effects_(effects),
        preview_(preview),
        test_quad_(test_quad),
        external_(external)
    {}

    void RendererLifecycle::OnMapReset()
    {
        if (terminal_)
        {
            return;
        }

        studio_.DiscardPending();
        preview_.DiscardCapturedWork();
        studio_.ClearDiagnostics();
        capture_.Cancel();
        editor_.End();
        external_.MapChanged();
        intercept_.ForgetModel();
        models_.ForgetSessionMetadata();
        effects_.Clear();
        models_.Clear();
        textures_.Clear();
    }

    void RendererLifecycle::OnDisconnectBegin()
    {
        studio_.DiscardPending();
        preview_.DiscardCapturedWork();
        studio_.ClearDiagnostics();
        capture_.Cancel();
        editor_.End();
    }

    void RendererLifecycle::OnClientStateClearing()
    {
        OnDisconnectBegin();
        intercept_.ForgetModel();
        models_.ForgetSessionMetadata();
    }

    void RendererLifecycle::BeginShutdown()
    {
        if (terminal_)
        {
            return;
        }

        // Owner calls can synchronously enter disconnect and client-state hooks.
        terminal_ = true;
        intercept_.Shutdown();
        preview_.RestoreStudioApi();
        OnDisconnectBegin();
    }

    void RendererLifecycle::ReleaseLiveContext()
    {
        if (context_transition_)
        {
            return;
        }

        if (context_handled_ && handled_generation_ == device_.get_generation())
        {
            if (terminal_)
            {
                FinishTerminalPhase();
            }

            return;
        }

        if (!device_.IsContextCurrent())
        {
            OnContextLost();

            if (terminal_)
            {
                FinishTerminalPhase();
            }

            return;
        }

        context_transition_ = true;
        context_handled_ = true;
        handled_generation_ = device_.get_generation();
        studio_.DiscardPending();
        effects_.Clear();
        preview_.FreeAll();
        intercept_.ForgetModel();

        if (terminal_)
        {
            models_.ForgetSessionMetadata();
        }

        models_.Clear();
        textures_.Clear();
        studio_.InvalidateContextReferences();
        studio_.ReleaseContextResources();
        test_quad_.Clear();
        shaders_.Clear();
        external_.ReleaseContext();
        if (terminal_)
        {
            external_.UninstallTextSurface();
        }

        external_.ReleaseText();
        device_.OnContextDestroyed();
        context_transition_ = false;

        if (terminal_)
        {
            FinishTerminalPhase();
        }
    }

    void RendererLifecycle::OnContextLost()
    {
        if (context_transition_ || (context_handled_ && handled_generation_ == device_.get_generation()))
        {
            return;
        }

        context_transition_ = true;
        context_handled_ = true;
        handled_generation_ = device_.get_generation();
        studio_.DiscardPending();
        preview_.DiscardCapturedWork();
        external_.AbandonContext();
        // Registered destructors must observe an unavailable device before dropping names.
        device_.OnContextDestroyed();
        intercept_.ForgetModel();
        effects_.Clear();
        studio_.InvalidateContextReferences();
        textures_.Forget();
        external_.ForgetText();
        context_transition_ = false;
        FinishTerminalPhase();
    }

    void RendererLifecycle::OnContextCreated()
    {
        if (terminal_ || context_transition_)
        {
            return;
        }

        void* current = context::Current();

        if (current == nullptr)
        {
            OnContextLost();
            return;
        }

        if (device_.IsContext(current) && device_.IsContextCurrent())
        {
            return;
        }

        OnContextLost();

        // Loss notifications can synchronously begin terminal module shutdown.
        if (terminal_)
        {
            return;
        }

        device_.OnContextCreated();
        context_handled_ = false;
        studio_.InvalidateContextReferences();
    }

    void RendererLifecycle::OnFatalShutdown()
    {
        if (fatal_requested_)
        {
            return;
        }

        fatal_requested_ = true;
        BeginShutdown();
        ReleaseLiveContext();
        FinishTerminalPhase();
    }

    void RendererLifecycle::FinishTerminalPhase()
    {
        if (!terminal_ || context_transition_)
        {
            return;
        }

        ClearTerminalOwners();

        if (fatal_requested_ && !fatal_complete_)
        {
            fatal_complete_ = true;
            external_.Shutdown();
        }
    }

    void RendererLifecycle::ClearTerminalOwners()
    {
        if (terminal_owners_cleared_)
        {
            return;
        }

        terminal_owners_cleared_ = true;
        intercept_.ForgetModel();
        models_.ForgetSessionMetadata();
        models_.Clear();
        preview_.FreeAll();
        external_.UninstallTextSurface();
        external_.ForgetText();
    }

} // namespace render
