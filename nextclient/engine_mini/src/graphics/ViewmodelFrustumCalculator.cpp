#include "ViewmodelFrustumCalculator.h"
#include "../engine.h"

void ViewmodelFrustumCalculator::CalcFrustum(GLdouble& left_out, GLdouble& right_out, GLdouble& bottom_out, GLdouble& top_out)
{
    bool fov_changed = false;

    if (old_cvar_fov_ != viewmodel_fov->value)
    {
        old_cvar_fov_ = viewmodel_fov->value;
        fov_changed = true;

        fov_ = std::clamp(viewmodel_fov->value, 1.f, 170.f);
        fov_ = CalcVerticalFov(fov_);
    }

    if (fov_changed || p_vid->width != old_vid_width_ || p_vid->height != old_vid_height_)
    {
        double aspect = (double)p_vid->width / (double)p_vid->height;
        double top = tan(DEG2RAD(fov_) / 2) * kZNear;

        old_vid_width_ = p_vid->width;
        old_vid_height_ = p_vid->height;

        left_ = -top * aspect;
        right_ = top * aspect;
        bottom_ = -top;
        top_ = top;
    }

    left_out = left_;
    right_out = right_;
    bottom_out = bottom_;
    top_out = top_;
}

float ViewmodelFrustumCalculator::CalcVerticalFov(float fov)
{
    /* hardcoded 4:3 aspect ratio so i don't need to do hor+ on vm fov */
    float x = 4.0f / tanf(DEG2RAD(fov) / 2);
    return RAD2DEG(atanf(3.0f / x)) * 2;
}