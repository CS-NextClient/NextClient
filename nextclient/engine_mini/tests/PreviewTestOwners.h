#pragma once

#include "PerformanceTestCore.h"

#include "engine.h"
#include "common/model.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/material/MaterialGameFiles.h"

namespace render::preview
{
    class WavefrontSceneTestAccess
    {
    public:
        static const std::string& Identifier(const WavefrontScene& scene)
        {
            return scene.batches_.front().identifier;
        }
    };

    class PreviewInteractionTestAccess
    {
    public:
        static size_t Count(const PreviewInteraction& interaction)
        {
            return interaction.packets_.size();
        }
        static const StudioDrawPacket& Packet(const PreviewInteraction& interaction, size_t index)
        {
            return interaction.packets_[index].draw;
        }
        static MaterialCoverageRuntime& Coverage(PreviewInteraction& interaction)
        {
            return interaction.coverage_;
        }
        static size_t RetainedBytes(const PreviewInteraction& interaction)
        {
            return interaction.mesh_storage_.get_retained_bytes();
        }
        static size_t IdentityCapacity(const PreviewInteraction& interaction)
        {
            return interaction.identities_.capacity();
        }
    };
} // namespace render::preview

namespace preview_test
{
    inline void BindEngine(render::RendererState& state)
    {
        state.preview_engine = {
            pStudioAPI,
            p_currententity,
            p_r_blend,
            mod_known,
            &mod_numknown,
            Mod_ForName,
            Mod_Extradata,
            cls,
            gEngfuncs.pfnGetScreenInfo,
            gEngfuncs.Con_Printf
        };
        state.preview_assets.BindFiles(gEngfuncs.COM_LoadFile, gEngfuncs.COM_FreeFile);
    }

    inline render::RendererLifecycle MakeLifecycle(render::RendererState& state)
    {
        return render::RendererLifecycle(
            state.resources.device,
            state.resources.shaders,
            state.resources.textures,
            state.models,
            state.editor,
            state.capture,
            state.studio,
            state.intercept,
            state.effects,
            state.preview,
            state.test_quad,
            render::GetRendererLifecycleBackend()
        );
    }

    inline render::preview::ScenePreview MakeScenePreview(render::RendererState& state)
    {
        return render::preview::ScenePreview(
            state.resources.device,
            state.resources.shaders,
            state.models,
            state.resources.textures,
            state.preview_renderer,
            state.preview_models,
            state.preview_engine,
            state.effects,
            state.coverage,
            state.preview_assets,
            state.studio
        );
    }

    struct Owners
    {
        performance_test::Core performance;
        render::RendererResources resources;
        render::MaterialGameFiles files;
        render::RendererState state;

        Owners() :
            resources(render::GetShaderBackend(), render::GetManifestBackend(), render::GetMaterialImageLoader()),
            state(
                resources,
                files,
                render::GetRendererLifecycleBackend(),
                render::GetRenderCaptureBackend(),
                performance.monitor,
                performance.mapper
            )
        {}
    };
} // namespace preview_test
