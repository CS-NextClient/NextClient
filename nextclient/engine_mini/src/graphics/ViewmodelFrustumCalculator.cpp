#include "ViewmodelFrustumCalculator.h"
#include "../engine.h"

void ViewmodelFrustumCalculator::CalcFrustum(GLdouble& left_out, GLdouble& right_out, GLdouble& bottom_out, GLdouble& top_out)
{
    bool fov_changed = false;

    static cvar_t* vm_fov_scale = nullptr;
    if (!vm_fov_scale)
        vm_fov_scale = gEngfuncs.pfnGetCvarPointer("_vm_fov_scale");

    float scale = (vm_fov_scale && vm_fov_scale->value > 0.0f) ? vm_fov_scale->value : 1.0f;
    float base_fov = viewmodel_fov->value * scale;

    if (cached_fov_ != base_fov)
    {
        cached_fov_ = base_fov;
        fov_changed = true;

        fov_ = std::clamp(base_fov, 1.f, 170.f);
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