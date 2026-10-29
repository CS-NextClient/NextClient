#pragma once

#include "graphics/preview/ScenePreview.h"
#include "graphics/preview/PreviewModelRepository.h"
#include "graphics/render/material/MaterialDocumentStore.h"
#include "graphics/render/material/MaterialEditor.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "graphics/render/effects/StudioEffects.h"
#include "graphics/render/RendererResources.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/model/StudioModelCache.h"
#include "graphics/render/render_state.h"
#include "graphics/render/StudioRenderer.h"
#include "graphics/render/StudioDiagnostics.h"
#include "graphics/render/TestQuad.h"
#include "graphics/render/RendererLifecycle.h"
#include "graphics/render/RenderCaptureSession.h"

namespace render
{
    struct RendererState
    {
        RendererResources& resources;
        MaterialCoverageRuntime coverage;
        StudioEffects effects;
        MaterialDocumentStore documents;
        ServerMaterials server_materials;
        StudioModelCache models;
        MaterialEditor editor;
        preview::PreviewEngineBindings preview_engine;
        preview::PreviewAssetBackend preview_assets;
        preview::EnginePreviewModelBackend preview_backend;
        preview::PreviewModelRepository preview_models;
        StudioEngineBindings studio_engine;
        StudioDiagnostics diagnostics;
        StudioRenderer studio;
        StudioInterceptState intercept;
        preview::PreviewModelRenderer preview_renderer;
        preview::ScenePreview preview;
        TestQuad test_quad;
        RenderCaptureSession capture;
        RendererLifecycle lifecycle;

        RendererState(
            RendererResources& resources,
            MaterialFileBackend& files,
            RendererLifecycleBackend& external,
            RenderCaptureBackend& capture_backend,
            performance::PerformanceMonitor& monitor,
            performance::PerformanceMapper& mapper
        ) :
            resources(resources),
            coverage(resources.manifests, resources.textures, monitor),
            effects(resources.device, resources.shaders, coverage),
            documents(files),
            server_materials(documents, resources.manifests),
            models(resources.device, documents, resources.manifests, resources.shaders, server_materials, monitor),
            editor(models, server_materials, resources.shaders, resources.manifests, documents),
            preview_backend(models, preview_engine, files),
            preview_models(preview_backend),
            diagnostics(studio_engine),
            studio(
                resources.device,
                resources.shaders,
                resources.manifests,
                resources.textures,
                models,
                server_materials,
                editor.get_session(),
                effects,
                studio_engine,
                diagnostics,
                coverage,
                monitor,
                mapper
            ),
            intercept(studio, studio_engine, monitor, studio.get_studio_depth()),
            preview_renderer(preview_models, preview_engine, studio),
            preview(
                resources.device,
                resources.shaders,
                models,
                resources.textures,
                preview_renderer,
                preview_models,
                preview_engine,
                effects,
                coverage,
                preview_assets,
                studio
            ),
            test_quad(resources.device, resources.shaders),
            capture(capture_backend),
            lifecycle(
                resources.device,
                resources.shaders,
                resources.textures,
                models,
                editor,
                capture,
                studio,
                intercept,
                effects,
                preview,
                test_quad,
                external
            )
        {}
    };

} // namespace render
