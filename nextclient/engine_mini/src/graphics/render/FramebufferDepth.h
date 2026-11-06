#pragma once

namespace render
{
    struct FramebufferDepthFormat_t
    {
        unsigned int internal_format{};
        int depth_bits{};
        int stencil_bits{};
        bool floating{};
    };

    // Queries the framebuffer already bound to target, including the default framebuffer.
    bool Framebuffer_QueryDepthFormat(unsigned int target, unsigned int framebuffer, FramebufferDepthFormat_t& result);
} // namespace render
