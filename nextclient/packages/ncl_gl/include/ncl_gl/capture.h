#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ncl_gl
{
    // A frame read back from GL: BGR triples, bottom left origin, which is the order
    // glReadPixels produces and the order a TGA descriptor of 0 declares.
    struct FrameImage
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> bgr;

        bool ok() const { return width > 0 && height > 0; }
    };

    // Reads whatever the scene is currently being drawn into, which is not always the
    // default framebuffer: fullscreen and -stretchaspect render through an FBO, and
    // reading the back buffer in those modes returns black. Restores every binding it
    // changes. An image with ok() == false means the viewport was too small to read;
    // the caller reports that in its own voice.
    FrameImage CaptureFramebuffer();

    bool WriteTga(const std::filesystem::path& path, const FrameImage& image);
}
