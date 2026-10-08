#pragma once

namespace view_bob
{
    inline constexpr int kStyleClassic = 0;
    inline constexpr int kStyleClassicSway = 1;
    inline constexpr int kStyleModern = 2;

    // units/s per second the modern bob lets the speed it follows change by
    inline constexpr float kModernSpeedAccel = 620.0f;

    struct BobParams
    {
        int style;          // kStyle*; any other value behaves as kStyleClassic
        float bob;          // cl_bob, world units per unit of horizontal speed
        float bob_cycle;    // cl_bobcycle, seconds per cycle; <= 0 disables the bob
        float bob_up;       // cl_bobup, fraction of the cycle spent rising, valid range (0, 1)
        float amt_vert;     // cl_bobamt_vert, modern vertical amplitude
        float amt_lat;      // cl_bobamt_lat, modern lateral amplitude
        float lower_amt;    // cl_bob_lower_amt, modern speed-proportional lowering
        bool camera_bob;    // cl_bob_camera, classic styles also move the view origin
    };

    struct ClassicBobState
    {
        double bob_time;
    };

    struct ModernBobState
    {
        float bob_time;
        float last_bob_time;
        float last_speed;
    };

    struct ModernBobOffsets
    {
        float vert;
        float hor;
    };

    // Where a bob step puts the view model: forward, up and to the side of the view in
    // world units, and the angles it turns by in degrees.
    struct BobOffsets
    {
        float forward;
        float up;
        float side;
        float pitch;
        float yaw;
        float roll;
    };

    // speed is the horizontal speed in units/s. Returns the offset in world units, clamped to [-7, 4].
    float StepClassicBob(ClassicBobState& state, const BobParams& params, float frametime, float speed);

    // time is absolute seconds. vert is clamped to [-8, 4], hor to [-7, 4], both in world units.
    ModernBobOffsets StepModernBob(ModernBobState& state, const BobParams& params, float time, float speed, bool onground);

    // style is the classic style the bob was stepped for: kStyleClassicSway turns the model as well
    BobOffsets PlaceClassicBob(float bob, int style);
    BobOffsets PlaceModernBob(const ModernBobOffsets& offsets);
}
