#include "graphics/render/RendererState.h"

#include "graphics/render/material/MaterialGameFiles.h"
#include "performance/PerformanceMonitor.h"
#include "performance/PerformanceMapper.h"
#include "graphics/render/render_resources.h"

namespace render
{
    namespace
    {
        struct RendererComposition
        {
            RendererResources& resources;
            MaterialGameFiles files;
            RendererState state;

            RendererComposition() :
                resources(GetResources()),
                state(
                    resources,
                    files,
                    GetRendererLifecycleBackend(),
                    GetRenderCaptureBackend(),
                    performance::GetMonitor(),
                    performance::GetMapper()
                )
            {}
        };

        RendererComposition& GetComposition()
        {
            static RendererComposition composition;
            return composition;
        }
    } // namespace

    RendererState& GetRendererState()
    {
        return GetComposition().state;
    }

    ServerMaterials& GetServerMaterials()
    {
        return GetComposition().state.server_materials;
    }

    MaterialEditor& GetMaterialEditor()
    {
        return GetComposition().state.editor;
    }

    MaterialDocumentStore& GetMaterialDocuments()
    {
        return GetComposition().state.documents;
    }

    void Renderer_InitializeMaterialFiles(const char* game_directory)
    {
        GetComposition().files.SetGameDirectory(game_directory);
    }
} // namespace render
