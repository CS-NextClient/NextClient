#pragma once

#include <vgui_controls/Panel.h>

#include <array>

#include <cvardef.h>
#include <ncl_math/color.h>
#include <next_engine_mini/ScenePreviewInterface.h>
#include <view/view_bob.h>

// Pending values of the non-bob view cvars the preview visualizes.
struct ViewTuningParams
{
    float offset_x;             // viewmodel_offset_x, flipped for the left hand like V_OffsetViewmodel
    float offset_y;             // viewmodel_offset_y, forward
    float offset_z;             // viewmodel_offset_z, positive moves the model up
    bool disable_shift;         // viewmodel_disable_shift
    float viewmodel_fov;        // degrees
    int lag_style;              // viewmodel_lag_style: 0 off, 1 HL2, 2 CS:S
    float lag_scale;
    float lag_speed;
    float roll_angle;           // cl_rollangle, degrees
    float roll_speed;           // cl_rollspeed, units/s
    float camera_move_scale;    // camera_movement_scale
    float camera_move_interp;   // camera_movement_interp, seconds
};

// Pending crosshair settings, mirroring what HudCrosshair reads from the cvars.
struct CrosshairParams
{
    int type;        // cl_crosshair_type
    ncl_math::Color color;
    int size_index;  // into crosshair::kSizes
    bool translucent;
    bool dynamic;
};

// Extra motion the preview runs to make the active settings tab readable.
enum class PreviewDemo
{
    kNone,
    kLag,           // camera looks left and right so the viewmodel lag shows
    kWeaponSwitch,  // periodic rifle/pistol switch with a draw animation and a camera kick
};

enum class PreviewMove
{
    kRun,
    kWalk,
    kStrafe,  // alternating left/right with short pauses
    kIdle,
};

// Animated first-person preview of the view settings, driven by the same view_bob math and
// cvar formulas the game uses on the pending dialog values, so it never touches the real
// cvars. The corridor and the view model are drawn by the client from the camera and the
// weapon placement handed to it; the panel itself paints the sky and the crosshair.
class CBobPreviewPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CBobPreviewPanel, vgui2::Panel);

    // the simulated player, driven by the movement buttons; eye_x counts to the right
    struct Movement
    {
        float forward_speed{};
        float side_speed{};
        float speed{};
        float eye_x{};
        int strafe_direction = -1;  // -1 left, +1 right; the demo starts leftward
        bool strafe_braking{};
        double strafe_pause_until{};
    };

    // the camera turn of the lag demo
    struct Sway
    {
        float yaw_deg{};
        bool sweeping_left = true;
        float phase{};  // sine argument of the leftward sweep
        double hold_until{};
    };

    struct WeaponSwitch
    {
        bool pistol_active{};
        float draw_lower_units{};
        float camera_pitch_deg{};
    };

    struct YawSample
    {
        float time;
        float yaw_deg;
    };

    struct Lag
    {
        float side{};
        float forward{};
        float last_front_x{};
        float last_front_y = 1.0f;

        // Sampled on a fixed step rather than once a frame, so the window the CS:S lag reads
        // back over does not shrink with the frame rate.
        std::array<YawSample, 64> history{};
        int history_next{};
        int history_count{};
        double history_time = -1.0;
    };

    // the dynamic gap, in crosshair scale-base pixels, before the resolution scaling
    struct Spread
    {
        int gap{};
        float distance{};
    };

private:
    // The corridor, loaded by the engine on the first draw; 0 while it is not.
    PreviewSceneHandle scene_{};
    bool scene_asked_{};

    view_bob::BobParams params_{};
    ViewTuningParams tuning_{};
    CrosshairParams crosshair_{};
    PreviewDemo demo_{};
    PreviewMove move_mode_{};

    Movement movement_{};
    Sway sway_{};
    WeaponSwitch weapon_switch_{};
    Lag lag_{};
    Spread spread_{};
    float camera_roll_deg_{};

    view_bob::ClassicBobState classic_state_{};
    view_bob::ModernBobState modern_state_{};
    float classic_bob_{};
    view_bob::ModernBobOffsets modern_offsets_{};

    double sim_time_{};
    double last_frame_time_ = -1.0;

    bool righthand_ = true;

    // Resolved once: reading a cvar by name walks the engine's list and parses the value
    // out of its string.
    cvar_t* righthand_cvar_{};
    cvar_t* fov_angle_{};
    cvar_t* fov_horplus_{};

public:
    CBobPreviewPanel(vgui2::Panel* parent, const char* panel_name);
    ~CBobPreviewPanel() override;

    void SetBobParams(const view_bob::BobParams& params);
    void SetViewTuning(const ViewTuningParams& params);
    void SetCrosshairParams(const CrosshairParams& params);
    void SetDemo(PreviewDemo demo);
    void SetMoveMode(PreviewMove mode);

    // Width over height of the video mode: the scene is projected through the panel's own
    // rectangle, so any other shape frames it differently from the player's screen.
    static float get_screen_aspect();

    void OnThink() override;
    void Paint() override;

private:
    void AdvanceSimulation();
    void AdvanceCamera(float frametime);
    void AdvanceLag(float frametime);
    view_bob::BobOffsets GetWeaponOffsets() const;
    float GetSceneFov() const;
    void DrawPreview(float camera_z_offset, const view_bob::BobOffsets& offsets);
    void DrawCrosshair();
};
