#pragma once

#include <string_view>

// Registration defaults of the cvars NextClient owns. The engine's cvar_t keeps only the
// current value, so nothing can ask the engine what a cvar was registered with: the
// modules that register these cvars and the settings UI share this table instead.
namespace cvars
{
    struct CvarDefault
    {
        const char* name;
        const char* value;
    };

    // registered by client_mini
    inline constexpr CvarDefault kViewmodelDisableShift{"viewmodel_disable_shift", "0"};
    inline constexpr CvarDefault kViewmodelOffsetX{"viewmodel_offset_x", "0"};
    inline constexpr CvarDefault kViewmodelOffsetY{"viewmodel_offset_y", "0"};
    inline constexpr CvarDefault kViewmodelOffsetZ{"viewmodel_offset_z", "0"};
    inline constexpr CvarDefault kBobStyle{"cl_bobstyle", "0"};
    inline constexpr CvarDefault kBobAmtVert{"cl_bobamt_vert", "0.13"};
    inline constexpr CvarDefault kBobAmtLat{"cl_bobamt_lat", "0.32"};
    inline constexpr CvarDefault kBobLowerAmt{"cl_bob_lower_amt", "8"};
    inline constexpr CvarDefault kBobCamera{"cl_bob_camera", "1"};
    inline constexpr CvarDefault kRollAngle{"cl_rollangle", "0"};
    inline constexpr CvarDefault kRollSpeed{"cl_rollspeed", "200"};
    inline constexpr CvarDefault kViewmodelLagStyle{"viewmodel_lag_style", "0"};
    inline constexpr CvarDefault kViewmodelLagScale{"viewmodel_lag_scale", "1.0"};
    inline constexpr CvarDefault kViewmodelLagSpeed{"viewmodel_lag_speed", "8.0"};
    inline constexpr CvarDefault kCameraMovementScale{"camera_movement_scale", "1"};
    inline constexpr CvarDefault kCameraMovementInterp{"camera_movement_interp", "0"};
    inline constexpr CvarDefault kCrosshairType{"cl_crosshair_type", "0"};

    // registered by engine_mini
    inline constexpr CvarDefault kViewmodelFov{"viewmodel_fov", "90.000000"};

    // registered by the game's own client dll with these values, which the settings UI
    // resets to
    inline constexpr CvarDefault kBob{"cl_bob", "0.01"};
    inline constexpr CvarDefault kBobCycle{"cl_bobcycle", "0.8"};
    inline constexpr CvarDefault kBobUp{"cl_bobup", "0.5"};
    inline constexpr CvarDefault kCrosshairSize{"cl_crosshair_size", "auto"};
    inline constexpr CvarDefault kCrosshairColor{"cl_crosshair_color", "50 250 50"};
    inline constexpr CvarDefault kCrosshairTranslucent{"cl_crosshair_translucent", "1"};
    inline constexpr CvarDefault kDynamicCrosshair{"cl_dynamiccrosshair", "1"};

    inline constexpr CvarDefault kAll[] = {
        kViewmodelDisableShift,
        kViewmodelOffsetX,
        kViewmodelOffsetY,
        kViewmodelOffsetZ,
        kBobStyle,
        kBobAmtVert,
        kBobAmtLat,
        kBobLowerAmt,
        kBobCamera,
        kRollAngle,
        kRollSpeed,
        kViewmodelLagStyle,
        kViewmodelLagScale,
        kViewmodelLagSpeed,
        kCameraMovementScale,
        kCameraMovementInterp,
        kCrosshairType,
        kViewmodelFov,
        kBob,
        kBobCycle,
        kBobUp,
        kCrosshairSize,
        kCrosshairColor,
        kCrosshairTranslucent,
        kDynamicCrosshair,
    };

    // nullptr for a cvar outside the table
    constexpr const char* FindDefault(std::string_view name)
    {
        for (const CvarDefault& entry : kAll)
        {
            if (name == entry.name)
                return entry.value;
        }

        return nullptr;
    }
}
