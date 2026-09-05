#include "BobPreviewPanel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui/IScheme.h>
#include <vgui/IPanel.h>
#include <crosshair/crosshair.h>
#include <ncl_math/vec3.h>
#include <tier2/tier2.h>
#include <view/view_fov.h>
#include <view/view_lag.h>

#include <GameUi.h>
#include "igameuifuncs.h"

namespace
{
    // eye height over the corridor floor, world units
    constexpr float kEyeHeight = 48.0f;

    // Releasing a movement key does not brake at a constant rate in game: ground friction
    // scales with the speed, so the slide fades out instead of ending on a corner. The
    // decay rate also sets the coasting distance (speed / rate, about 45 units at a run).
    constexpr float kStopFriction = 5.5f;            // 1/s
    constexpr float kStopSpeed = 2.0f;               // units/s counted as standing still

    // The lag demo sweeps left across the whole range at a sine pace, then snaps back
    // right several times faster, so the two directions read differently.
    constexpr float kLagSwayAmplitudeDeg = 40.0f;
    constexpr float kLagSweepRate = 4.0f;            // rad/s of the sine argument
    constexpr float kLagSnapDegPerSec = 360.0f;
    constexpr float kLagHoldTime = 0.5f;
    constexpr float kLagSwayFadeDegPerSec = 60.0f;

    // How often the view angle is written into the history: 64 samples then reach back
    // 0.64 s whatever the frame rate, where one sample a frame covered less than the delay
    // above 640 fps and left the lag reading its own current angle.
    constexpr float kLagSampleStep = 0.01f;

    // weapon switch demo: rifle and pistol alternate, the draw raises the weapon and
    // wiggles the camera the way the camera bone of a draw animation does
    constexpr float kSwitchPeriod = 3.0f;
    constexpr float kSwitchDrawDuration = 0.5f;
    constexpr float kSwitchDrawLowerUnits = 7.0f;
    constexpr float kSwitchKickDuration = 0.8f;
    constexpr float kSwitchKickDeg = 1.8f;
    constexpr float kSwitchKickRate = 16.0f;
    constexpr float kSwitchKickDamping = 5.0f;

    // strafe demo: left first at a run, then bouncing between symmetric extremes with
    // pauses; the reversal triggers on position, so the extremes match exactly on both sides
    constexpr float kStrafePauseTime = 0.3f;
    constexpr float kStrafeAmplitude = 14.5f;        // reversal point; the coast adds ~45 more

    // the camera travels at the simulated velocity, as it would in game; the limit is a
    // guard that keeps it inside the corridor rather than a pacing knob
    constexpr float kStrafeTravelLimit = 70.0f;      // world units
    constexpr float kStrafeRecenterSpeed = 120.0f;   // units/s outside the strafe mode

    constexpr float kRunSpeed = 250.0f;
    constexpr float kWalkSpeed = 140.0f;

    constexpr float kDisableShiftRaise = 1.0f;  // world units

    // sky, painted first; the corridor is drawn over it
    const Color kColorSky(158, 189, 214, 255);

    // What the far wall of the corridor comes out as. The translucent crosshair is drawn on
    // top of it and adds to it, the way the additive fill in game does.
    const Color kColorFarWall(181, 150, 103, 255);

    // The view models the preview stands in for. Both ship with the game, so the panel does
    // not carry a model of its own.
    constexpr char kRifleModel[] = "models/v_ak47.mdl";
    constexpr char kPistolModel[] = "models/v_glock18.mdl";

    // The idle sequence of a view model, which is the pose a settings preview shows.
    constexpr int kIdleSequence = 0;

    // Where the weapon stands in the preview before the view settings move it: camera
    // space, forward, to the right and up. A matter of how the panel is composed, not of
    // what the player will see in game.
    constexpr float kWeaponForward = -1.0f;
    constexpr float kWeaponRight = -1.0f;
    constexpr float kWeaponUp = -0.2f;

    // The corridor the weapon stands in, and the archives its textures are looked up in.
    constexpr char kScenePath[] = "resource/preview/corridor.obj";
    constexpr const char* kSceneWads[] = {"cs_dust.wad", "cstrike.wad", "halflife.wad"};

    // The sun of the preview, travelling down from above and the right and a little in
    // front, so the floor, the left wall and the sides of the crates that face the camera
    // and the right catch it; warm like the sand. The textures carry baked light of their
    // own, so the span the corridor gets is narrow; the weapon takes the studio renderer's
    // terms.
    constexpr PreviewLight kSun = {
        {-0.2494f, 0.5486f, -0.7980f},
        {1.0f, 0.95f, 0.85f},
        0.72f,
        0.28f,
        96,
        140,
    };

    float CvarValue(const cvar_t* cvar)
    {
        return cvar != nullptr ? cvar->value : 0.0f;
    }
}

CBobPreviewPanel::CBobPreviewPanel(vgui2::Panel* parent, const char* panel_name) :
    vgui2::Panel(parent, panel_name)
{
    SetPaintBackgroundEnabled(false);
    SetMouseInputEnabled(false);
    SetKeyBoardInputEnabled(false);

    righthand_cvar_ = engine->pfnGetCvarPointer("cl_righthand");
    fov_angle_ = engine->pfnGetCvarPointer("fov_angle");
    fov_horplus_ = engine->pfnGetCvarPointer("fov_horplus");
}

CBobPreviewPanel::~CBobPreviewPanel()
{
    if (scene_ != 0 && ScenePreview() != nullptr)
    {
        ScenePreview()->FreeScene(scene_);
    }
}

void CBobPreviewPanel::SetBobParams(const view_bob::BobParams& params)
{
    params_ = params;
}

void CBobPreviewPanel::SetViewTuning(const ViewTuningParams& params)
{
    tuning_ = params;
}

void CBobPreviewPanel::SetCrosshairParams(const CrosshairParams& params)
{
    crosshair_ = params;
}

void CBobPreviewPanel::SetDemo(PreviewDemo demo)
{
    demo_ = demo;
}

void CBobPreviewPanel::SetMoveMode(PreviewMove mode)
{
    if (move_mode_ == mode)
        return;

    move_mode_ = mode;

    if (mode == PreviewMove::kStrafe)
    {
        movement_.strafe_direction = -1;
        movement_.strafe_braking = false;
        movement_.strafe_pause_until = 0.0;
    }
}

void CBobPreviewPanel::OnThink()
{
    BaseClass::OnThink();

    AdvanceSimulation();
    Repaint();
}

void CBobPreviewPanel::AdvanceSimulation()
{
    double now = vgui2::system()->GetFrameTime();

    if (last_frame_time_ < 0.0)
        last_frame_time_ = now;

    float frametime = std::clamp(static_cast<float>(now - last_frame_time_), 0.0f, 0.1f);
    last_frame_time_ = now;

    sim_time_ += frametime;

    float target_forward = 0.0f;
    float target_side = 0.0f;

    switch (move_mode_)
    {
        case PreviewMove::kRun:
        {
            target_forward = kRunSpeed;
            break;
        }
        case PreviewMove::kWalk:
        {
            target_forward = kWalkSpeed;
            break;
        }
        case PreviewMove::kStrafe:
        {
            if (sim_time_ < movement_.strafe_pause_until)
                break;

            if (movement_.strafe_braking)
            {
                // the camera keeps sliding after the reversal, so the pause is timed
                // from the moment it actually stands still at the far point
                if (movement_.side_speed == 0.0f)
                {
                    movement_.strafe_braking = false;
                    movement_.strafe_pause_until = sim_time_ + kStrafePauseTime;
                }

                break;
            }

            bool reached = movement_.strafe_direction < 0
                ? movement_.eye_x <= -kStrafeAmplitude
                : movement_.eye_x >= kStrafeAmplitude;

            if (reached)
            {
                movement_.strafe_direction = -movement_.strafe_direction;
                movement_.strafe_braking = true;
                break;
            }

            target_side = static_cast<float>(movement_.strafe_direction) * kRunSpeed;
            break;
        }
        case PreviewMove::kIdle:
        {
            break;
        }
    }

    float max_delta = view_bob::kModernSpeedAccel * frametime;
    float stop_decay = expf(-kStopFriction * frametime);

    auto follow_target = [&](float current, float target) {
        if (target != 0.0f)
            return current + std::clamp(target - current, -max_delta, max_delta);

        float slowed = current * stop_decay;

        return fabsf(slowed) <= kStopSpeed ? 0.0f : slowed;
    };

    movement_.forward_speed = follow_target(movement_.forward_speed, target_forward);
    movement_.side_speed = follow_target(movement_.side_speed, target_side);
    movement_.speed = sqrtf(movement_.forward_speed * movement_.forward_speed + movement_.side_speed * movement_.side_speed);

    movement_.eye_x += movement_.side_speed * frametime;

    if (move_mode_ != PreviewMove::kStrafe)
    {
        float recenter = kStrafeRecenterSpeed * frametime;
        movement_.eye_x -= std::clamp(movement_.eye_x, -recenter, recenter);
    }

    movement_.eye_x = std::clamp(movement_.eye_x, -kStrafeTravelLimit, kStrafeTravelLimit);

    righthand_ = righthand_cvar_ == nullptr || righthand_cvar_->value != 0.0f;

    spread_.gap = crosshair::kRifleGap;
    if (crosshair_.dynamic && movement_.speed > crosshair::kRifleRunSpeed)
        spread_.gap = static_cast<int>(spread_.gap * crosshair::kRunSpread);

    spread_.distance = crosshair::Decay(spread_.distance, frametime);
    if (spread_.distance < spread_.gap)
        spread_.distance = static_cast<float>(spread_.gap);

    AdvanceCamera(frametime);
    AdvanceLag(frametime);

    if (params_.style == view_bob::kStyleModern)
    {
        modern_offsets_ = view_bob::StepModernBob(modern_state_, params_, static_cast<float>(sim_time_), movement_.speed, true);
        classic_bob_ = 0.0f;
    }
    else
    {
        classic_bob_ = view_bob::StepClassicBob(classic_state_, params_, frametime, movement_.speed);
        modern_offsets_ = view_bob::ModernBobOffsets{};
    }
}

void CBobPreviewPanel::AdvanceCamera(float frametime)
{
    bool lag_demo_running = demo_ == PreviewDemo::kLag && tuning_.lag_style != 0;

    if (!lag_demo_running)
    {
        float fade_delta = kLagSwayFadeDegPerSec * frametime;
        sway_.yaw_deg -= std::clamp(sway_.yaw_deg, -fade_delta, fade_delta);

        // the next run picks the sweep up from wherever the view ended up
        sway_.sweeping_left = true;
        sway_.phase = ncl_math::kPi - asinf(std::clamp(sway_.yaw_deg / kLagSwayAmplitudeDeg, -1.0f, 1.0f));
        sway_.hold_until = 0.0;
    }
    else if (sim_time_ >= sway_.hold_until)
    {
        if (sway_.sweeping_left)
        {
            sway_.phase += kLagSweepRate * frametime;

            if (sway_.phase >= ncl_math::kPi * 1.5f)
            {
                sway_.yaw_deg = -kLagSwayAmplitudeDeg;
                sway_.sweeping_left = false;
                sway_.hold_until = sim_time_ + kLagHoldTime;
            }
            else
            {
                sway_.yaw_deg = kLagSwayAmplitudeDeg * sinf(sway_.phase);
            }
        }
        else
        {
            sway_.yaw_deg += kLagSnapDegPerSec * frametime;

            if (sway_.yaw_deg >= kLagSwayAmplitudeDeg)
            {
                sway_.yaw_deg = kLagSwayAmplitudeDeg;
                sway_.sweeping_left = true;
                sway_.phase = ncl_math::kPi * 0.5f;
                sway_.hold_until = sim_time_ + kLagHoldTime;
            }
        }
    }

    weapon_switch_.camera_pitch_deg = 0.0f;
    weapon_switch_.pistol_active = false;
    weapon_switch_.draw_lower_units = 0.0f;

    if (demo_ == PreviewDemo::kWeaponSwitch)
    {
        float switch_time = static_cast<float>(fmod(sim_time_, static_cast<double>(kSwitchPeriod)));

        weapon_switch_.pistol_active = static_cast<int>(sim_time_ / static_cast<double>(kSwitchPeriod)) % 2 == 1;

        if (switch_time < kSwitchDrawDuration)
        {
            float remaining = 1.0f - switch_time / kSwitchDrawDuration;
            weapon_switch_.draw_lower_units = kSwitchDrawLowerUnits * remaining * remaining;
        }

        if (switch_time < kSwitchKickDuration)
        {
            // starts displaced like the camera bone of a draw animation, so with zero
            // interp the view snaps and camera_movement_interp visibly removes the snap
            float kick = kSwitchKickDeg * cosf(switch_time * kSwitchKickRate) * expf(-switch_time * kSwitchKickDamping);

            float ease = 1.0f;
            if (tuning_.camera_move_interp > 0.0f)
                ease = std::min(1.0f, switch_time / tuning_.camera_move_interp);

            weapon_switch_.camera_pitch_deg = kick * ease * tuning_.camera_move_scale;
        }
    }

    // V_CalcRoll applied to the simulated lateral velocity
    float side_abs = fabsf(movement_.side_speed);
    float roll = tuning_.roll_angle;

    if (side_abs < tuning_.roll_speed)
        roll = side_abs * tuning_.roll_angle / tuning_.roll_speed;

    camera_roll_deg_ = movement_.side_speed >= 0.0f ? roll : -roll;
}

void CBobPreviewPanel::AdvanceLag(float frametime)
{
    int history_size = static_cast<int>(lag_.history.size());

    if (lag_.history_time < 0.0 || sim_time_ - lag_.history_time >= kLagSampleStep)
    {
        lag_.history_time = sim_time_;

        lag_.history[lag_.history_next] = YawSample{static_cast<float>(sim_time_), sway_.yaw_deg};
        lag_.history_next = (lag_.history_next + 1) % history_size;

        if (lag_.history_count < history_size)
        {
            lag_.history_count++;
        }
    }

    lag_.side = 0.0f;
    lag_.forward = 0.0f;

    float front_x = sinf(sway_.yaw_deg * ncl_math::kDeg2Rad);
    float front_y = cosf(sway_.yaw_deg * ncl_math::kDeg2Rad);

    if (tuning_.lag_style == 1)
    {
        // yaw-only adaptation of V_AddLag_HL2
        float delta_x = front_x - lag_.last_front_x;
        float delta_y = front_y - lag_.last_front_y;

        lag_.last_front_x += delta_x * tuning_.lag_speed * frametime;
        lag_.last_front_y += delta_y * tuning_.lag_speed * frametime;

        lag_.side = -delta_x * tuning_.lag_scale;
        lag_.forward = -delta_y * tuning_.lag_scale;
    }
    else
    {
        lag_.last_front_x = front_x;
        lag_.last_front_y = front_y;
    }

    if (tuning_.lag_style == 2)
    {
        // yaw-only adaptation of V_AddLag_CSS: react to the view direction 0.1 s ago
        float past_time = static_cast<float>(sim_time_) - view_lag::kCssDelay;
        float past_yaw = sway_.yaw_deg;

        // The history is a ring in time order, so the samples around that moment are a few
        // steps back from the newest; walking back, the first one at or before it is the
        // nearer side and the one passed just before it the farther.
        const YawSample* after = nullptr;

        for (int back = 1; back <= lag_.history_count; back++)
        {
            const YawSample& sample = lag_.history[(lag_.history_next - back + history_size) % history_size];

            if (sample.time > past_time)
            {
                after = &sample;
                continue;
            }

            past_yaw = sample.yaw_deg;

            if (after != nullptr && after->time > sample.time)
            {
                float frac = (past_time - sample.time) / (after->time - sample.time);
                past_yaw = sample.yaw_deg + (after->yaw_deg - sample.yaw_deg) * frac;
            }

            break;
        }

        float delta_yaw = (sway_.yaw_deg - past_yaw) * ncl_math::kDeg2Rad;

        lag_.side = sinf(delta_yaw) * tuning_.lag_scale;
        lag_.forward = (1.0f - cosf(delta_yaw)) * tuning_.lag_scale;
    }
}

void CBobPreviewPanel::Paint()
{
    int wide, tall;
    GetSize(wide, tall);

    vgui2::surface()->DrawSetColor(kColorSky);
    vgui2::surface()->DrawFilledRect(0, 0, wide, tall);

    float camera_dz = 0.0f;
    if (params_.style != view_bob::kStyleModern && params_.camera_bob)
        camera_dz = classic_bob_;

    DrawPreview(camera_dz, GetWeaponOffsets());
    DrawCrosshair();
}

view_bob::BobOffsets CBobPreviewPanel::GetWeaponOffsets() const
{
    view_bob::BobOffsets offsets = params_.style == view_bob::kStyleModern
        ? view_bob::PlaceModernBob(modern_offsets_)
        : view_bob::PlaceClassicBob(classic_bob_, params_.style);

    offsets.side += righthand_ ? tuning_.offset_x : -tuning_.offset_x;
    offsets.forward += tuning_.offset_y;
    offsets.up += tuning_.offset_z;

    if (tuning_.disable_shift)
        offsets.up += kDisableShiftRaise;

    offsets.up -= weapon_switch_.draw_lower_units;

    offsets.side += lag_.side;
    offsets.forward += lag_.forward;

    return offsets;
}

float CBobPreviewPanel::get_screen_aspect()
{
    int wide = 0;
    int tall = 0;
    vgui2::surface()->GetScreenSize(wide, tall);

    // while the surface cannot answer for the video mode
    if (wide <= 0 || tall <= 0)
    {
        return 16.0f / 9.0f;
    }

    return static_cast<float>(wide) / static_cast<float>(tall);
}

// Horizontal, degrees, at the base zoom: the client scales this field by the zoom of a
// scoped weapon, which the preview never holds.
float CBobPreviewPanel::GetSceneFov() const
{
    float fov = view_fov::kDefault;

    if (fov_angle_ != nullptr)
    {
        fov = std::clamp(fov_angle_->value, view_fov::kMin, view_fov::kMax);
    }

    return view_fov::ScreenFov(fov, get_screen_aspect(), CvarValue(fov_horplus_) != 0.0f);
}

// The corridor and the weapon are drawn by the engine, which the panel hands its camera, its
// light and the weapon's placement. The offsets are the ones CalcCustomRefdef applies to the
// view model in game, so the preview moves by the same arithmetic the player will get.
void CBobPreviewPanel::DrawPreview(float camera_z_offset, const view_bob::BobOffsets& offsets)
{
    ScenePreviewInterface* preview = ScenePreview();
    if (preview == nullptr)
    {
        return;
    }

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);

    if (wide <= 0 || tall <= 0)
    {
        return;
    }

    // Asked for once: a file that was not there is read again by the engine's reload
    // command, not by the next paint.
    if (!scene_asked_)
    {
        scene_asked_ = true;
        scene_ = preview->LoadScene(kScenePath, kSceneWads, static_cast<int>(std::size(kSceneWads)));
    }

    PreviewDrawParams params{};

    // The panel counts its lateral position to the right, the world to the left.
    params.camera.origin[0] = 0.0f;
    params.camera.origin[1] = -movement_.eye_x;
    params.camera.origin[2] = kEyeHeight + camera_z_offset;
    params.camera.angles[0] = weapon_switch_.camera_pitch_deg;
    params.camera.angles[1] = sway_.yaw_deg;
    params.camera.angles[2] = camera_roll_deg_;
    params.camera.fov = GetSceneFov();
    params.light = kSun;
    params.scene = scene_;
    params.clear = false;

    PreviewModel weapon{};
    weapon.path = weapon_switch_.pistol_active ? kPistolModel : kRifleModel;
    weapon.sequence = kIdleSequence;
    weapon.frame = 0.0f;

    weapon.origin[0] = offsets.forward + kWeaponForward;
    weapon.origin[1] = -(offsets.side + kWeaponRight);
    weapon.origin[2] = offsets.up + kWeaponUp;
    weapon.angles[0] = offsets.pitch;
    weapon.angles[1] = offsets.yaw;
    weapon.angles[2] = offsets.roll;
    weapon.camera_space = true;
    // horizontal, like the engine's; the vertical field is derived on the engine side, the
    // way the game derives the view model's
    weapon.fov = std::clamp(tuning_.viewmodel_fov, 40.0f, 140.0f);
    weapon.mirror = righthand_;

    params.models = &weapon;
    params.model_count = 1;

    PreviewRect rect;
    rect.x = 0;
    rect.y = 0;
    LocalToScreen(rect.x, rect.y);
    rect.wide = wide;
    rect.tall = tall;

    // What of the panel is actually on screen: vgui2 clips its own drawing to this and
    // knows nothing about the one below, which goes straight to the frame buffer.
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    vgui2::ipanel()->GetClipRect(GetVPanel(), x0, y0, x1, y1);

    PreviewRect clip;
    clip.x = x0;
    clip.y = y0;
    clip.wide = x1 - x0;
    clip.tall = y1 - y0;

    preview->Draw(params, rect, clip);
}

// Mirrors HudCrosshair: same colors, per-resolution scaling and bar layout, with the
// previewed rifle standing in for the actual weapon.
void CBobPreviewPanel::DrawCrosshair()
{
    int wide, tall;
    GetSize(wide, tall);

    int center_x = wide / 2;
    int center_y = tall / 2;

    int r = crosshair_.color.r;
    int g = crosshair_.color.g;
    int b = crosshair_.color.b;

    int screen_wide = 0;
    int screen_tall = 0;
    int screen_bpp = 0;
    g_pGameUIFuncs->GetCurrentVideoMode(&screen_wide, &screen_tall, &screen_bpp);

    if (screen_wide <= 0)
        screen_wide = 640;

    int scale_base = crosshair::ScaleBase(std::clamp(crosshair_.size_index, 0, crosshair::kSizeCount - 1), screen_wide);

    int bar_size = crosshair::BarSize(spread_.distance, spread_.gap);
    float distance = spread_.distance;

    if (screen_wide != scale_base)
    {
        distance = crosshair::ScaledDistance(spread_.distance, screen_wide, scale_base);
        bar_size = crosshair::ScaledBarSize(bar_size, screen_wide, scale_base);
    }

    // The game draws this crosshair with pfnFillRGBA, which adds to the pixels behind it
    // rather than blending with them. The surface offers no additive fill and its additive
    // line draw does not reach the screen here, so the sum is worked out instead: the
    // centre of the view looks down the corridor, so the far wall is what it lands on.
    if (crosshair_.translucent)
    {
        r = std::min(255, r + kColorFarWall.r());
        g = std::min(255, g + kColorFarWall.g());
        b = std::min(255, b + kColorFarWall.b());
    }

    vgui2::surface()->DrawSetColor(Color(r, g, b, 255));

    auto fill = [](int x, int y, int w, int h) {
        vgui2::surface()->DrawFilledRect(x, y, x + w, y + h);
    };

    int type = std::clamp(crosshair_.type, 0, crosshair::kTypeCount - 1);

    if (type == crosshair::kTypeCircle)
    {
        float radius = static_cast<float>(bar_size / 2) + distance;
        int count = static_cast<int>(cosf(ncl_math::kPi / 4.0f) * radius + 0.5f);

        for (int i = 0; i < count; i++)
        {
            int size = static_cast<int>(sqrtf(std::max(0.0f, radius * radius - static_cast<float>(i * i))));

            fill(center_x + i, center_y + size, 1, 1);
            fill(center_x + i, center_y - size, 1, 1);
            fill(center_x - i, center_y + size, 1, 1);
            fill(center_x - i, center_y - size, 1, 1);
            fill(center_x + size, center_y + i, 1, 1);
            fill(center_x + size, center_y - i, 1, 1);
            fill(center_x - size, center_y + i, 1, 1);
            fill(center_x - size, center_y - i, 1, 1);
        }
    }
    else if (type == crosshair::kTypeDot)
    {
        fill(center_x - 1, center_y - 1, 3, 3);
    }
    else
    {
        int gap = static_cast<int>(distance);

        fill(center_x + gap, center_y, bar_size, 1);
        fill(center_x - gap - bar_size + 1, center_y, bar_size, 1);
        fill(center_x, center_y + gap, 1, bar_size);

        if (type != crosshair::kTypeT)
            fill(center_x, center_y - gap - bar_size + 1, 1, bar_size);
    }
}
