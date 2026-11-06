#pragma once

#include <vector>

#include <next_engine_mini/ScenePreviewInterface.h>

#include "graphics/render/RenderDevice.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/FixedShaderProgram.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"

namespace render::preview
{
    class PreviewInteraction : public render::IDeviceResource
    {
    private:
        friend class PreviewInteractionTestAccess;

        struct Packet
        {
            render::StudioDrawPacket draw;
            int model;
            bool camera_space;
            bool mirror;
        };

        RenderDevice& device_;
        ShaderManager& shaders_;
        MaterialCoverageRuntime& coverage_;
        FixedShaderProgram pick_program_;
        FixedShaderProgram outline_program_;
        uint32_t binding_generation_{};
        unsigned int bones_block_{};
        MaterialCoverageBindings coverage_bindings_;
        std::vector<Packet> packets_;
        StudioMeshStorage mesh_storage_{1024 * 1024};
        std::vector<PreviewHit> identities_;
        int model_{};
        bool camera_space_{};
        bool mirror_{};
        unsigned int framebuffer_{};
        unsigned int ids_{};
        unsigned int depth_{};
        unsigned int bones_{};
        unsigned int vao_{};
        int wide_{};
        int tall_{};

    public:
        PreviewInteraction(RenderDevice& device, ShaderManager& shaders, MaterialCoverageRuntime& coverage);
        ~PreviewInteraction() override;
        PreviewInteraction(const PreviewInteraction&) = delete;
        PreviewInteraction& operator=(const PreviewInteraction&) = delete;
        void BeginFrame();
        void BeginModel(int index, const PreviewModel& model);
        void Capture(const render::StudioDrawPacket& packet);
        PreviewHit Render(const PreviewRect& view, const PreviewRect& clip, int x, int y);
        void DiscardPackets();
        void Clear();
        void OnDeviceLost() override;

    private:
        bool EnsureTarget(int wide, int tall);
    };

} // namespace render::preview
