#include "GameUi/MaterialEditor/MaterialPreviewState.h"

#include <algorithm>
#include <cmath>

namespace material_editor
{

namespace
{
    constexpr float kMaximumFrameTime = 0.1f;
    constexpr float kSunDegreesPerSecond = 15.0f;
    constexpr float kSunOrbitElevation = 35.0f;
    constexpr float kPanRate = 0.002f;
    constexpr float kZoomRate = 0.12f;
    constexpr float kMinimumDistance = 4.0f;
    constexpr float kMaximumDistance = 4096.0f;

    float FrameElapsed(double now, double previous)
    {
        return previous >= 0.0 && now >= previous ? std::clamp(static_cast<float>(now - previous), 0.0f, kMaximumFrameTime) : 0.0f;
    }
} // namespace

void MaterialPreviewState::Reset()
{
    *this = MaterialPreviewState{};
}

void MaterialPreviewState::SetModel(const char* path, const PreviewModelInfo& info)
{
    model_ = path;
    info_ = info;
    sequence_ = 0;
    frame_ = 0.0f;
    sequence_seconds_ = 1.0f;
    last_time_ = -1.0;
    skin_ = 0;
    body_ = 0;
    FrameModel();
}

void MaterialPreviewState::SetSequence(int sequence, float seconds)
{
    sequence_ = std::max(sequence, 0);
    sequence_seconds_ = std::isfinite(seconds) && seconds > 0.0f ? seconds : 1.0f;
    frame_ = 0.0f;
    last_time_ = -1.0;
}

void MaterialPreviewState::SetFrame(float frame)
{
    if (std::isfinite(frame))
    {
        frame_ = std::clamp(frame, 0.0f, 1.0f);
    }
}

void MaterialPreviewState::SetPlaying(bool playing)
{
    if (playing_ != playing)
    {
        last_time_ = -1.0;
        playing_ = playing;
    }
}

void MaterialPreviewState::SetPlaybackRate(float rate)
{
    if (std::isfinite(rate))
    {
        playback_rate_ = std::clamp(rate, 0.25f, 2.0f);
    }
}

float MaterialPreviewState::get_playback_rate() const
{
    return playback_rate_;
}

void MaterialPreviewState::SetPresentation(int skin, int body, float pitch, float yaw, float scale)
{
    skin_ = std::max(skin, 0);
    body_ = std::max(body, 0);

    if (std::isfinite(pitch) && std::isfinite(yaw) && std::isfinite(scale))
    {
        pitch_ = std::clamp(pitch, -89.0f, 89.0f);
        yaw_ = std::fmod(yaw, 360.0f);
        scale_ = std::clamp(scale, 0.05f, 10.0f);
    }
}

void MaterialPreviewState::SetWeaponView(bool weapon_view)
{
    weapon_view_ = weapon_view;
}

void MaterialPreviewState::SetMirror(bool mirror)
{
    mirror_ = mirror;
}

void MaterialPreviewState::Advance(double now)
{
    elapsed_ = 0.0f;

    if (!std::isfinite(now))
    {
        return;
    }

    elapsed_ = FrameElapsed(now, last_time_);
    last_time_ = now;

    if (playing_)
    {
        frame_ = std::fmod(frame_ + elapsed_ * playback_rate_ / sequence_seconds_, 1.0f);
    }

    AdvanceSun(now);
}

void MaterialPreviewState::AdvanceSun(double now)
{
    if (!sun_animating_)
    {
        return;
    }

    float elapsed = FrameElapsed(now, last_sun_time_);
    last_sun_time_ = now;
    light_azimuth_ = std::fmod(light_azimuth_ + kSunDegreesPerSecond * sun_speed_ * elapsed, 360.0f);
}

void MaterialPreviewState::Orbit(float yaw, float pitch)
{
    if (weapon_view_)
    {
        yaw_ = std::fmod(yaw_ + yaw, 360.0f);
        pitch_ = std::clamp(pitch_ + pitch, -89.0f, 89.0f);
        return;
    }

    orbit_yaw_ = std::fmod(orbit_yaw_ + yaw, 360.0f);
    orbit_pitch_ = std::clamp(orbit_pitch_ + pitch, -89.0f, 89.0f);
}

void MaterialPreviewState::Pan(float horizontal, float vertical)
{
    if (weapon_view_)
    {
        weapon_offset_.y -= horizontal * 0.02f;
        weapon_offset_.z += vertical * 0.02f;
        return;
    }

    PreviewCamera camera = BuildCamera();
    ncl_math::Vector3 forward;
    ncl_math::Vector3 right;
    ncl_math::Vector3 up;
    ncl_math::AngleVectors(camera.angles, forward.data(), right.data(), up.data());
    target_ = target_ + (right * horizontal + up * vertical) * (distance_ * kPanRate);
}

void MaterialPreviewState::Zoom(float steps)
{
    if (weapon_view_)
    {
        weapon_fov_ = std::clamp(weapon_fov_ * std::exp(-steps * kZoomRate), 40.0f, 140.0f);
        return;
    }

    distance_ = std::clamp(distance_ * std::exp(-steps * kZoomRate), kMinimumDistance, kMaximumDistance);
}

void MaterialPreviewState::FrameModel()
{
    weapon_offset_ = ncl_math::Vector3{};
    weapon_fov_ = 90.0f;
    ncl_math::Vector3 minimum(info_.bounds_min[0], info_.bounds_min[1], info_.bounds_min[2]);
    ncl_math::Vector3 maximum(info_.bounds_max[0], info_.bounds_max[1], info_.bounds_max[2]);
    ncl_math::Vector3 center = (minimum + maximum) * 0.5f;
    ncl_math::Vector3 forward;
    ncl_math::Vector3 right;
    ncl_math::Vector3 up;
    // StudioSetUpTransform negates entity pitch before building the model matrix.
    float angles[] = {-pitch_, yaw_, 0.0f};
    ncl_math::AngleVectors(angles, forward.data(), right.data(), up.data());
    target_ = (forward * center.x - right * center.y + up * center.z) * scale_;
    float radius = (maximum - minimum).Length() * 0.5f;
    radius = radius > 0.0f ? radius : 24.0f;
    distance_ = std::clamp(radius * scale_ * 2.8f, kMinimumDistance, kMaximumDistance);
}

void MaterialPreviewState::ResetView()
{
    weapon_offset_ = ncl_math::Vector3{};
    weapon_fov_ = 90.0f;

    if (weapon_view_)
    {
        pitch_ = 0.0f;
        yaw_ = 0.0f;
    }
    else
    {
        orbit_yaw_ = 0.0f;
        orbit_pitch_ = 15.0f;
        FrameModel();
    }
}

void MaterialPreviewState::SetSunGizmo(bool shown)
{
    sun_gizmo_ = shown;
}

void MaterialPreviewState::SetSunAnimating(bool animating)
{
    if (sun_animating_ != animating)
    {
        sun_animating_ = animating;
        last_sun_time_ = -1.0;

        if (animating && (light_elevation_ < sun_orbit::kMinimumElevation || light_elevation_ > sun_orbit::kMaximumElevation))
        {
            light_elevation_ = kSunOrbitElevation;
        }
    }
}

void MaterialPreviewState::SetSunSpeed(float rate)
{
    if (std::isfinite(rate))
    {
        sun_speed_ = std::clamp(rate, sun_orbit::kMinimumSpeed, sun_orbit::kMaximumSpeed);
    }
}

float MaterialPreviewState::get_sun_speed() const
{
    return sun_speed_;
}

bool MaterialPreviewState::is_sun_animating() const
{
    return sun_animating_;
}

bool MaterialPreviewState::is_sun_gizmo_shown() const
{
    return sun_gizmo_;
}

void MaterialPreviewState::SetLightAngles(float azimuth, float elevation)
{
    if (std::isfinite(azimuth) && std::isfinite(elevation))
    {
        light_azimuth_ = std::fmod(std::fmod(azimuth, 360.0f) + 360.0f, 360.0f);
        light_elevation_ = sun_animating_ ? std::clamp(elevation, sun_orbit::kMinimumElevation, sun_orbit::kMaximumElevation)
                                          : std::clamp(elevation, -90.0f, 90.0f);
    }
}

void MaterialPreviewState::SetLightColor(int red, int green, int blue)
{
    light_color_ = {std::clamp(red, 0, 255), std::clamp(green, 0, 255), std::clamp(blue, 0, 255)};
}

void MaterialPreviewState::SetLightStrengths(float ambient, float directional)
{
    if (std::isfinite(ambient) && std::isfinite(directional))
    {
        ambient_ = std::clamp(ambient, 0.0f, 1.0f);
        directional_ = std::clamp(directional, 0.0f, 1.0f);
    }
}

void MaterialPreviewState::SetLightPreset(MaterialLightPreset preset)
{
    SetLightColor(255, 255, 255);

    switch (preset)
    {
        case MaterialLightPreset::AmbientOnly:
            SetLightAngles(0.0f, 90.0f);
            SetLightStrengths(1.0f, 0.0f);
            break;
        case MaterialLightPreset::Grazing:
            SetLightAngles(0.0f, 0.0f);
            SetLightStrengths(0.15f, 0.85f);
            break;
        default:
            SetLightAngles(0.0f, 90.0f);
            SetLightStrengths(0.5f, 0.5f);
            break;
    }
}

PreviewCamera MaterialPreviewState::BuildCamera() const
{
    float yaw = orbit_yaw_ * ncl_math::kDeg2Rad;
    float pitch = orbit_pitch_ * ncl_math::kDeg2Rad;
    PreviewCamera camera{};
    camera.origin[0] = target_.x + distance_ * std::cos(pitch) * std::cos(yaw);
    camera.origin[1] = target_.y + distance_ * std::cos(pitch) * std::sin(yaw);
    camera.origin[2] = target_.z + distance_ * std::sin(pitch);
    camera.angles[0] = orbit_pitch_;
    camera.angles[1] = orbit_yaw_ + 180.0f;
    camera.fov = 60.0f;
    return camera;
}

PreviewModel MaterialPreviewState::BuildModel() const
{
    PreviewModel model{};
    model.path = model_.c_str();
    model.sequence = sequence_;
    model.frame = frame_;
    model.angles[0] = pitch_;
    model.angles[1] = yaw_;
    model.camera_space = weapon_view_;
    model.fov = weapon_fov_;

    if (weapon_view_)
    {
        std::copy(weapon_offset_.data(), weapon_offset_.data() + 3, model.origin);
    }
    model.mirror = mirror_;
    model.scale = scale_;
    model.skin = skin_;
    model.body = body_;
    return model;
}

PreviewLight MaterialPreviewState::BuildLight() const
{
    float azimuth = light_azimuth_ * ncl_math::kDeg2Rad;
    float elevation = light_elevation_ * ncl_math::kDeg2Rad;
    PreviewLight light{};
    light.direction[0] = -std::cos(elevation) * std::cos(azimuth);
    light.direction[1] = -std::cos(elevation) * std::sin(azimuth);
    light.direction[2] = -std::sin(elevation);

    for (int index = 0; index < 3; index++)
    {
        light.color[index] = static_cast<float>(light_color_[index]) / 255.0f;
    }

    light.ambient = ambient_;
    light.diffuse = directional_;
    light.model_ambient = static_cast<int>(std::round(128.0f * ambient_));
    light.model_shade = static_cast<int>(std::round(255.0f * directional_));
    return light;
}

const char* MaterialPreviewState::get_model() const
{
    return model_.c_str();
}

const PreviewModelInfo& MaterialPreviewState::get_model_info() const
{
    return info_;
}

int MaterialPreviewState::get_sequence() const
{
    return sequence_;
}

float MaterialPreviewState::get_frame() const
{
    return frame_;
}

float MaterialPreviewState::get_elapsed() const
{
    return elapsed_;
}

float MaterialPreviewState::get_pitch() const
{
    return pitch_;
}

float MaterialPreviewState::get_yaw() const
{
    return yaw_;
}

float MaterialPreviewState::get_scale() const
{
    return scale_;
}

bool MaterialPreviewState::is_playing() const
{
    return playing_;
}

bool MaterialPreviewState::is_weapon_view() const
{
    return weapon_view_;
}

float MaterialPreviewState::get_light_azimuth() const
{
    return light_azimuth_;
}

float MaterialPreviewState::get_light_elevation() const
{
    return light_elevation_;
}

const std::array<int, 3>& MaterialPreviewState::get_light_color() const
{
    return light_color_;
}

float MaterialPreviewState::get_ambient() const
{
    return ambient_;
}

float MaterialPreviewState::get_directional() const
{
    return directional_;
}

void MaterialPreviewState::SetOccluder(bool shown)
{
    occluder_ = shown;
}

bool MaterialPreviewState::is_occluder_shown() const
{
    return occluder_;
}

} // namespace material_editor
