#pragma once

#include "graphics/render/effects/StudioEffectTargets.h"
#include "graphics/render/FixedShaderProgram.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"

namespace ncl_gl
{
    struct Program;
}

namespace render
{
    class MaterialCoverageRuntime;
    struct StudioDrawPacket;

    class StudioEffectPasses
    {
    public:
        class Composition
        {
        private:
            friend class StudioEffectPasses;

            const ncl_gl::Program* seed_{};
            const ncl_gl::Program* distance_{};
            const ncl_gl::Program* composite_{};
            int output_{};
            effects::Rect_t clip_{};
            bool clipped_{};
        };

    private:
        struct MaskBindings
        {
            unsigned int bones{};
            int chrome_origin{};
            int view_right{};
            int base{};
            int texel_scale{};
            int chrome{};
            int masked{};
            int alpha_scale{};
            int fill{};
            int outline{};
            int outline_mode{};
            int shape{};
            MaterialCoverageBindings coverage;
        };

        struct DepthBindings
        {
            int object_depth{};
            int scene_depth{};
            int object_depth_ms{};
            int scene_depth_ms{};
            int samples{};
            int origin{};
            int sign{};
            int tolerance{};
        };

        struct SeedBindings
        {
            DepthBindings depth;
            int shape{};
        };

        struct DistanceBindings
        {
            int step{};
            int seeds{};
        };

        struct CompositeBindings
        {
            DepthBindings depth;
            int fill{};
            int outline{};
            int shape{};
            int seeds{};
            int output_origin{};
        };

        StudioEffectTargets& targets_;
        ShaderManager& shaders_;
        MaterialCoverageRuntime& coverage_;
        FixedShaderProgram mask_;
        FixedShaderProgram seed_;
        FixedShaderProgram distance_;
        FixedShaderProgram composite_;
        uint32_t mask_generation_{};
        uint32_t composition_generation_{};
        MaskBindings mask_bindings_;
        SeedBindings seed_bindings_;
        DistanceBindings distance_bindings_;
        CompositeBindings composite_bindings_;

    public:
        StudioEffectPasses(StudioEffectTargets& targets, ShaderManager& shaders, MaterialCoverageRuntime& coverage);

        const ncl_gl::Program* PrepareCapture();
        void CaptureMask(const StudioEffectTargets::Mask& target, const StudioDrawPacket& packet, const ncl_gl::Program& mask);
        void CaptureDepth(const StudioEffectTargets::Mask& target, const StudioDrawPacket& packet, const ncl_gl::Program& mask);
        bool PrepareComposition(const effects::Rect_t& viewport, Composition& composition);
        void Seed(const StudioEffectTargets::Mask& target, const effects::Rect_t& viewport, const Composition& composition);
        int PropagateSeeds(const effects::Rect_t& viewport, const Composition& composition);
        void Composite(
            const StudioEffectTargets::Mask& target,
            const effects::Rect_t& viewport,
            int seed_index,
            const Composition& composition
        );

    private:
        void DrawMeshes(const StudioDrawPacket& packet, const effects::Rect_t& viewport);

        static DepthBindings PrepareDepth(const ncl_gl::Program& program);
        static void BindDepth(
            const DepthBindings& bindings,
            unsigned int object,
            unsigned int scene,
            int samples,
            int object_unit,
            int scene_unit
        );
    };
} // namespace render
