#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "graphics/render/effects/studio_effect_bounds.h"

namespace render
{
    class RenderDevice;

    class StudioEffectTargets
    {
    public:
        class Mask
        {
        private:
            friend class StudioEffectTargets;

            uintptr_t entity_{};
            effects::Rect_t rect_{};
            unsigned int framebuffer_{};
            unsigned int depth_framebuffer_{};
            unsigned int fill_{};
            unsigned int outline_{};
            unsigned int shape_{};
            unsigned int mask_depth_{};
            unsigned int object_depth_{};
            bool reversed_{};
            int samples_{};

        public:
            Mask() = default;
            Mask(const Mask&) = delete;
            Mask& operator=(const Mask&) = delete;

            uintptr_t get_entity() const
            {
                return entity_;
            }

            const effects::Rect_t& get_rect() const
            {
                return rect_;
            }

            unsigned int get_framebuffer() const
            {
                return framebuffer_;
            }

            unsigned int get_depth_framebuffer() const
            {
                return depth_framebuffer_;
            }

            unsigned int get_fill() const
            {
                return fill_;
            }

            unsigned int get_outline() const
            {
                return outline_;
            }

            unsigned int get_shape() const
            {
                return shape_;
            }

            unsigned int get_object_depth() const
            {
                return object_depth_;
            }

            bool is_reversed() const
            {
                return reversed_;
            }

            int get_samples() const
            {
                return samples_;
            }

        private:
            void Reset(const effects::Rect_t& area, bool reverse);
            bool Allocate(const effects::Rect_t& area, bool reverse, int sample_count);
            void Release(bool release_gl);
        };

        using Masks_t = std::vector<std::unique_ptr<Mask>>;

    private:
        RenderDevice& device_;
        Masks_t pool_;
        unsigned int bones_{};
        unsigned int vao_{};
        unsigned int scene_framebuffer_{};
        unsigned int scene_depth_{};
        unsigned int capture_depth_framebuffer_{};
        unsigned int capture_depth_image_{};
        int capture_depth_wide_{};
        int capture_depth_tall_{};
        int capture_depth_samples_{};
        unsigned int seed_framebuffers_[2]{};
        unsigned int seeds_[2]{};
        int scene_wide_{};
        int scene_tall_{};
        int scene_samples_{};
        unsigned int scene_format_{};
        float depth_tolerance_{};
        int seed_wide_{};
        int seed_tall_{};

    public:
        explicit StudioEffectTargets(RenderDevice& device);
        StudioEffectTargets(const StudioEffectTargets&) = delete;
        StudioEffectTargets& operator=(const StudioEffectTargets&) = delete;

        Mask* AcquireMask(Masks_t& masks, uintptr_t entity, const effects::Rect_t& bounds, bool reversed, int samples, bool& reused);
        void RecycleMasks(Masks_t& masks);
        void ReleaseMasks(Masks_t& masks, bool release_gl);
        void Clear(bool release_gl);
        void EnsureGeometry();
        bool EnsureCaptureDepth(const effects::Rect_t& viewport, int samples);
        bool SnapshotDepth(unsigned int output, const effects::Rect_t& viewport);
        void EnsureSeeds(int wide, int tall);

        unsigned int get_bones() const
        {
            return bones_;
        }

        unsigned int get_vertex_array() const
        {
            return vao_;
        }

        unsigned int get_capture_depth_framebuffer() const
        {
            return capture_depth_framebuffer_;
        }

        unsigned int get_scene_depth() const
        {
            return scene_depth_;
        }

        unsigned int get_scene_format() const
        {
            return scene_format_;
        }

        int get_scene_samples() const
        {
            return scene_samples_;
        }

        float get_depth_tolerance() const
        {
            return depth_tolerance_;
        }

        unsigned int get_seed_framebuffer(int index) const
        {
            return seed_framebuffers_[index];
        }

        unsigned int get_seed_image(int index) const
        {
            return seeds_[index];
        }

    private:
        void PreserveMask(const Mask& source, const Mask& destination);
    };
} // namespace render
