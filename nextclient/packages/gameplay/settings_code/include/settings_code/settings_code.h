#pragma once

#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include <cvars/cvar_defaults.h>

// A share code for the Game options tab. The values themselves are packed into the
// string, so a player can hand their crosshair, bobbing and the rest to someone else
// without a config file or a server to look the code up on.
namespace settings_code
{
    enum Section
    {
        kCrosshair,
        kBobbing,
        kModel,
        kInertia,
        kCamera,
        kSectionCount
    };

    inline constexpr uint8_t kAllSections = (1 << kSectionCount) - 1;

    enum FieldId
    {
        kCrosshairType,
        kCrosshairSize,
        kCrosshairColorR,
        kCrosshairColorG,
        kCrosshairColorB,
        kCrosshairTranslucent,
        kDynamicCrosshair,

        kBobStyle,
        kBob,
        kBobCycle,
        kBobUp,
        kBobAmtVert,
        kBobAmtLat,
        kBobLowerAmt,

        kViewmodelOffsetX,
        kViewmodelOffsetY,
        kViewmodelOffsetZ,
        kViewmodelFov,
        kViewmodelDisableShift,

        kLagStyle,
        kLagScale,
        kLagSpeed,

        kRollAngle,
        kRollSpeed,
        kCameraMovementScale,
        kCameraMovementInterp,
        kBobCamera,

        kFieldCount
    };

    // A value is stored as its number of steps above min, so the ranges and steps match
    // the tab's sliders and combos. cl_crosshair_size is stored as its index into
    // crosshair::kSizes, and cl_crosshair_color takes three fields, one per channel.
    struct Field
    {
        Section section;
        const char* cvar;
        float min;
        float max;
        float step;
    };

    // In FieldId order: this order is the code format, so new fields only ever go at the
    // end of a section together with a new format version.
    inline constexpr Field kFields[] = {
        {kCrosshair, cvars::kCrosshairType.name, 0, 3, 1},
        {kCrosshair, cvars::kCrosshairSize.name, 0, 4, 1},
        {kCrosshair, cvars::kCrosshairColor.name, 0, 255, 1},
        {kCrosshair, cvars::kCrosshairColor.name, 0, 255, 1},
        {kCrosshair, cvars::kCrosshairColor.name, 0, 255, 1},
        {kCrosshair, cvars::kCrosshairTranslucent.name, 0, 1, 1},
        {kCrosshair, cvars::kDynamicCrosshair.name, 0, 1, 1},

        {kBobbing, cvars::kBobStyle.name, 0, 2, 1},
        {kBobbing, cvars::kBob.name, 0, 0.05f, 0.001f},
        {kBobbing, cvars::kBobCycle.name, 0.1f, 2, 0.01f},
        {kBobbing, cvars::kBobUp.name, 0.05f, 0.95f, 0.01f},
        {kBobbing, cvars::kBobAmtVert.name, 0, 0.4f, 0.01f},
        {kBobbing, cvars::kBobAmtLat.name, 0, 0.8f, 0.01f},
        {kBobbing, cvars::kBobLowerAmt.name, 0, 30, 1},

        {kModel, cvars::kViewmodelOffsetX.name, -8, 8, 0.01f},
        {kModel, cvars::kViewmodelOffsetY.name, -8, 8, 0.01f},
        {kModel, cvars::kViewmodelOffsetZ.name, -8, 8, 0.01f},
        {kModel, cvars::kViewmodelFov.name, 70, 100, 1},
        {kModel, cvars::kViewmodelDisableShift.name, 0, 1, 1},

        {kInertia, cvars::kViewmodelLagStyle.name, 0, 2, 1},
        {kInertia, cvars::kViewmodelLagScale.name, 0, 5, 0.01f},
        {kInertia, cvars::kViewmodelLagSpeed.name, 1, 20, 0.1f},

        {kCamera, cvars::kRollAngle.name, 0, 10, 0.1f},
        {kCamera, cvars::kRollSpeed.name, 10, 400, 1},
        {kCamera, cvars::kCameraMovementScale.name, 0, 2, 0.01f},
        {kCamera, cvars::kCameraMovementInterp.name, 0, 0.5f, 0.01f},
        {kCamera, cvars::kBobCamera.name, 0, 1, 1},
    };

    static_assert(std::size(kFields) == kFieldCount);

    using Values = std::array<float, kFieldCount>;

    struct Decoded
    {
        uint8_t sections;
        // only the fields of the sections in the code are set
        Values values;
    };

    // Values out of a field's range are clamped and rounded to its step.
    std::string Encode(const Values& values, uint8_t sections);

    // nullopt for anything that isn't a whole, intact code of this format version.
    std::optional<Decoded> Decode(std::string_view code);
}
