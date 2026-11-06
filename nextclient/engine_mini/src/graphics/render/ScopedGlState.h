#pragma once

#include <array>
#include <cstdint>

#include "graphics/TextureBindingState.h"
#include "graphics/render/StudioBindings.h"

namespace render
{
    class RenderDevice;
    class ScopedGlState
    {
    private:
        RenderDevice* device_{};
        uint32_t generation_{};
        const uint64_t* cancellation_epoch_{};
        uint64_t expected_cancellation_epoch_{};
        TextureBindingCacheState texture_cache_;
        int matrix_mode_{};
        int program_{};
        int vao_{};
        int array_buffer_{};
        int element_buffer_{};
        int uniform_buffer_{};
        int bones_binding_{};
        int64_t bones_start_{};
        int64_t bones_size_{};
        int renderbuffer_{};
        int pixel_pack_buffer_{};
        int pixel_unpack_buffer_{};
        std::array<int, render::kStudioTouchedTextureUnits> samplers_{};
        std::array<int, render::kStudioTouchedTextureUnits> multisample_textures_{};
        int active_texture_{};
        int read_framebuffer_{};
        int draw_framebuffer_{};
        bool modern_{};

    public:
        ScopedGlState();
        explicit ScopedGlState(RenderDevice& device, const uint64_t* cancellation_epoch = nullptr);
        ~ScopedGlState();

        ScopedGlState(const ScopedGlState&) = delete;
        ScopedGlState& operator=(const ScopedGlState&) = delete;
    };
} // namespace render
