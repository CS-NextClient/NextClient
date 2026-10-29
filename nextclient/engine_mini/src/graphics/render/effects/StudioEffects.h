#pragma once

#include <cstddef>
#include <vector>

#include "graphics/render/effects/StudioEffectPasses.h"
#include "graphics/render/render_device.h"

namespace ncl_material
{
    struct Material;
}

namespace render
{
    struct StudioGpuModel;

    struct StudioEffectsStats
    {
        uint32_t entities{};
        uint32_t packets{};
        uint32_t reused_targets{};
        uint64_t mask_pixels{};
        unsigned int depth_format{};
        int samples{};
    };

    class StudioEffects final : public IDeviceResource
    {
    private:
        struct View
        {
            uint64_t identity{};
            StudioEffectTargets::Masks_t masks;
            effects::Rect_t viewport{};
            StudioEffectsStats stats{};
        };

        RenderDevice& device_;
        MaterialCoverageRuntime& coverage_;
        StudioEffectTargets targets_;
        StudioEffectPasses passes_;
        std::vector<View> views_;
        uint64_t next_view_identity_{};
        StudioEffectsStats last_stats_{};

    public:
        StudioEffects(RenderDevice& device, ShaderManager& shaders, MaterialCoverageRuntime& coverage);
        ~StudioEffects() override;
        StudioEffects(const StudioEffects&) = delete;
        StudioEffects& operator=(const StudioEffects&) = delete;

        bool CheckEnabled(const ncl_material::Material& material) const;
        bool CheckEnabled(const StudioDrawPacket& packet) const;
        effects::Rect_t ProjectBounds(const StudioGpuModel& model, const StudioDrawPacket& packet) const;
        uint64_t BeginView();
        void CancelView(uint64_t identity);
        size_t get_view_count() const;
        void Capture(uintptr_t entity, const StudioDrawPacket& packet, const effects::Rect_t& bounds, bool include_occluders = false);
        void EndView();
        void Clear();
        bool Contains(uintptr_t entity) const;
        StudioEffectsStats get_last_stats() const;
        size_t get_target_count() const;
        void OnDeviceLost() override;

    private:
        void Reset(bool release_gl);
    };
} // namespace render
