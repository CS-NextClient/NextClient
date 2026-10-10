#pragma once

#include <glad/glad.h>

class MapRenderState
{
private:
    GLint unit_{};
    GLint textures_[2]{};
    GLint program_{};
    GLint draw_framebuffer_{};
    GLint read_framebuffer_{};
    GLint renderbuffer_{};
    GLint matrix_mode_{};
    bool split_framebuffers_{};

public:
    MapRenderState();
    ~MapRenderState();

    MapRenderState(const MapRenderState&) = delete;
    MapRenderState& operator=(const MapRenderState&) = delete;
};
