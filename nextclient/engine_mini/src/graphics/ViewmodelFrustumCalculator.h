#pragma once
#include "qgl.hpp"

class ViewmodelFrustumCalculator
{
public:
    static constexpr GLdouble kZNear = 1.0; /* was 4 but that's too far for viewmodels */
    static constexpr GLdouble kZFar = 4096.0;

private:
    float cached_fov_{};
    unsigned int old_vid_width_{};
    unsigned int old_vid_height_{};

    float fov_{};

    GLdouble left_{};
    GLdouble right_{};
    GLdouble bottom_{};
    GLdouble top_{};

public:
    void CalcFrustum(GLdouble& left, GLdouble& right, GLdouble& bottom, GLdouble& top);

    // The vertical field a horizontal one is drawn through, over a fixed 4:3 so that a
    // view model keeps its size whatever the screen is shaped like.
    static float CalcVerticalFov(float fov);
};
