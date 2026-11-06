#pragma once

#include <array>
#include <string>

#include <ncl_math/vec3.h>
#include <next_engine_mini/ScenePreviewInterface.h>

namespace material_editor
{
namespace sun_orbit
{
    constexpr float kMinimumElevation = 5.0f;
    constexpr float kMaximumElevation = 80.0f;
    constexpr float kMinimumSpeed = 0.25f;
    constexpr float kMaximumSpeed = 4.0f;
} // namespace sun_orbit

enum class MaterialLightPreset
{
    Neutral,
    AmbientOnly,
    Grazing
};

class MaterialPreviewState
{
private:
    std::string model_;
    PreviewModelInfo info_{};
    ncl_math::Vector3 target_{};
    ncl_math::Vector3 weapon_offset_{};
    float weapon_fov_ = 90.0f;
    float orbit_yaw_{};
    float orbit_pitch_ = 15.0f;
    float distance_ = 64.0f;
    float pitch_{};
    float yaw_{};
    float scale_ = 1.0f;
    int skin_{};
    int body_{};
    int sequence_{};
    float frame_{};
    float sequence_seconds_ = 1.0f;
    float playback_rate_ = 1.0f;
    double last_time_ = -1.0;
    float elapsed_{};
    bool playing_{};
    bool weapon_view_{};
    bool mirror_{};
    bool sun_gizmo_ = true;
    bool occluder_{};
    bool sun_animating_{};
    double last_sun_time_ = -1.0;
    float sun_speed_ = 1.0f;
    float light_azimuth_{};
    float light_elevation_ = 90.0f;
    std::array<int, 3> light_color_ = {255, 255, 255};
    float ambient_ = 0.5f;
    float directional_ = 0.5f;

public:
    void Reset();
    void SetModel(const char* path, const PreviewModelInfo& info);
    void SetSequence(int sequence, float seconds);
    void SetFrame(float frame);
    void SetPlaying(bool playing);
    void SetPlaybackRate(float rate);
    void SetPresentation(int skin, int body, float pitch, float yaw, float scale);
    void SetWeaponView(bool weapon_view);
    void SetMirror(bool mirror);
    void Advance(double now);
    void Orbit(float yaw, float pitch);
    void Pan(float horizontal, float vertical);
    void Zoom(float steps);
    void FrameModel();
    void ResetView();
    void SetSunGizmo(bool shown);
    void SetOccluder(bool shown);
    void SetSunAnimating(bool animating);
    void SetSunSpeed(float rate);
    void SetLightAngles(float azimuth, float elevation);
    void SetLightColor(int red, int green, int blue);
    void SetLightStrengths(float ambient, float directional);
    void SetLightPreset(MaterialLightPreset preset);

    PreviewCamera BuildCamera() const;
    PreviewModel BuildModel() const;
    PreviewLight BuildLight() const;

    const char* get_model() const;
    const PreviewModelInfo& get_model_info() const;
    int get_sequence() const;
    float get_frame() const;
    float get_elapsed() const;
    float get_playback_rate() const;
    float get_pitch() const;
    float get_yaw() const;
    float get_scale() const;
    bool is_playing() const;
    bool is_weapon_view() const;
    bool is_sun_gizmo_shown() const;
    bool is_occluder_shown() const;
    bool is_sun_animating() const;
    float get_sun_speed() const;
    float get_light_azimuth() const;
    float get_light_elevation() const;
    const std::array<int, 3>& get_light_color() const;
    float get_ambient() const;
    float get_directional() const;

private:
    void AdvanceSun(double now);
};

} // namespace material_editor
