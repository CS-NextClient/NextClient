#include "ncl_gl/capture.h"

#include <fstream>

#include <glad/glad.h>

namespace ncl_gl
{
    FrameImage CaptureFramebuffer()
    {
        FrameImage image;

        GLint viewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, viewport);

        int width = viewport[2];
        int height = viewport[3];

        if (width < 4 || height < 4)
        {
            return image;
        }

        GLint previous_read_fb = 0;
        GLint previous_draw_fb = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_fb);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fb);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, previous_draw_fb);

        GLint previous_read_buffer = GL_BACK;
        glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
        glReadBuffer(previous_draw_fb == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);

        GLint previous_alignment = 4;
        glGetIntegerv(GL_PACK_ALIGNMENT, &previous_alignment);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        image.width = width;
        image.height = height;
        image.bgr.resize(static_cast<size_t>(width) * height * 3);

        glReadPixels(viewport[0], viewport[1], width, height, GL_BGR, GL_UNSIGNED_BYTE, image.bgr.data());

        glPixelStorei(GL_PACK_ALIGNMENT, previous_alignment);
        glReadBuffer(previous_read_buffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, previous_read_fb);

        return image;
    }

    bool WriteTga(const std::filesystem::path& path, const FrameImage& image)
    {
        if (!image.is_valid())
        {
            return false;
        }

        std::ofstream file(path, std::ios::binary);

        if (!file)
        {
            return false;
        }

        uint8_t header[18] = {};
        header[2] = 2; // uncompressed true-color
        header[12] = static_cast<uint8_t>(image.width & 0xFF);
        header[13] = static_cast<uint8_t>((image.width >> 8) & 0xFF);
        header[14] = static_cast<uint8_t>(image.height & 0xFF);
        header[15] = static_cast<uint8_t>((image.height >> 8) & 0xFF);
        header[16] = 24;
        // descriptor 0: bottom-left origin, matching the glReadPixels row order
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        file.write(reinterpret_cast<const char*>(image.bgr.data()), static_cast<std::streamsize>(image.bgr.size()));

        return static_cast<bool>(file);
    }
} // namespace ncl_gl
