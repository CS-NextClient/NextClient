#include "GameUi/MaterialEditor/MaterialLightPanel.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <iterator>

#include <tier1/KeyValues.h>
#include <tier1/strtools.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Slider.h>

namespace material_editor
{

using namespace vgui2;

namespace
{
    struct LightField
    {
        const char* name;
        const char* label;
        int minimum;
        int maximum;
    };

    constexpr LightField kLightFields[] = {
        {"Azimuth", "#GameUI_MatEdit_LightAzimuth", 0, 360},
        {"Elevation", "#GameUI_MatEdit_LightElevation", -90, 90},
        {"Red", "#GameUI_MatEdit_LightRed", 0, 255},
        {"Green", "#GameUI_MatEdit_LightGreen", 0, 255},
        {"Blue", "#GameUI_MatEdit_LightBlue", 0, 255},
        {"Ambient", "#GameUI_MatEdit_LightAmbient", 0, 1000},
        {"Directional", "#GameUI_MatEdit_LightDirectional", 0, 1000},
    };
} // namespace

CMaterialLightPanel::CMaterialLightPanel(Panel* parent, MaterialPreviewState& state) :
    BaseClass(parent, "MaterialLighting"),
    state_(state)
{
    preset_ = new ComboBox(this, "LightPreset", 4, false);
    preset_->AddItem("#GameUI_MatEdit_LightNeutral", nullptr);
    preset_->AddItem("#GameUI_MatEdit_LightAmbientOnly", nullptr);
    preset_->AddItem("#GameUI_MatEdit_LightGrazing", nullptr);
    preset_->AddItem("#GameUI_MatEdit_LightCustom", nullptr);
    preset_->AddActionSignalTarget(this);

    sun_ = new CheckButton(this, "ShowSunGizmo", "#GameUI_MatEdit_ShowSunGizmo");
    sun_->AddActionSignalTarget(this);

    occluder_ = new CheckButton(this, "ShowOccluder", "#GameUI_MatEdit_ShowOccluder");
    occluder_->AddActionSignalTarget(this);

    animate_sun_ = new CheckButton(this, "AnimateSun", "#GameUI_MatEdit_AnimateSun");
    animate_sun_->AddActionSignalTarget(this);

    sun_speed_.label = new Label(this, "SunSpeedLabel", "#GameUI_MatEdit_SunSpeed");
    sun_speed_.slider = new Slider(this, "SunSpeed");
    sun_speed_.slider->SetRange(
        static_cast<int>(sun_orbit::kMinimumSpeed * 100.0f), static_cast<int>(sun_orbit::kMaximumSpeed * 100.0f)
    );
    sun_speed_.slider->SetNumTicks(0);
    sun_speed_.slider->AddActionSignalTarget(this);
    sun_speed_.value = new Label(this, "SunSpeedValue", "");

    for (size_t index = 0; index < lights_.size(); index++)
    {
        const LightField& field = kLightFields[index];
        LightControl& control = lights_[index];
        control.label = new Label(this, field.name, field.label);
        control.slider = new Slider(this, field.name);
        control.slider->SetRange(field.minimum, field.maximum);
        control.slider->SetNumTicks(0);
        control.slider->AddActionSignalTarget(this);
        control.value = new Label(this, field.name, "");
    }

    Synchronize();
}

void CMaterialLightPanel::PerformLayout()
{
    BaseClass::PerformLayout();
    preset_->SetBounds(0, 0, GetWide(), 22);
    sun_->SetBounds(0, 28, GetWide(), 20);
    animate_sun_->SetBounds(0, 50, GetWide(), 20);
    occluder_->SetBounds(0, 72, GetWide(), 20);
    sun_speed_.label->SetBounds(0, 98, 112, 20);
    sun_speed_.slider->SetBounds(114, 98, std::max(GetWide() - 170, 32), 22);
    sun_speed_.value->SetBounds(GetWide() - 52, 98, 52, 20);

    for (size_t index = 0; index < lights_.size(); index++)
    {
        int y = 126 + static_cast<int>(index) * 28;
        lights_[index].label->SetBounds(0, y, 112, 20);
        lights_[index].slider->SetBounds(114, y, std::max(GetWide() - 170, 32), 22);
        lights_[index].value->SetBounds(GetWide() - 52, y, 52, 20);
    }
}

void CMaterialLightPanel::Synchronize()
{
    lights_[1].slider->SetRange(
        state_.is_sun_animating() ? static_cast<int>(sun_orbit::kMinimumElevation) : -90,
        state_.is_sun_animating() ? static_cast<int>(sun_orbit::kMaximumElevation) : 90
    );

    if (!sun_speed_.slider->IsDragged())
    {
        sun_speed_.slider->SetValue(static_cast<int>(std::round(state_.get_sun_speed() * 100.0f)), false);
    }

    const std::array<int, 3>& color = state_.get_light_color();
    const int values[] = {
        static_cast<int>(std::round(state_.get_light_azimuth())),
        static_cast<int>(std::round(state_.get_light_elevation())),
        color[0],
        color[1],
        color[2],
        static_cast<int>(std::round(state_.get_ambient() * 1000.0f)),
        static_cast<int>(std::round(state_.get_directional() * 1000.0f)),
    };

    for (size_t index = 0; index < lights_.size(); index++)
    {
        if (!lights_[index].slider->IsDragged())
        {
            lights_[index].slider->SetValue(values[index], false);
        }
    }

    if (state_.is_sun_animating())
    {
        selected_preset_ = 3;
    }

    preset_->SilentActivateItemByRow(selected_preset_);
    sun_->SilentSetSelected(state_.is_sun_gizmo_shown());
    occluder_->SilentSetSelected(state_.is_occluder_shown());
    animate_sun_->SilentSetSelected(state_.is_sun_animating());
    UpdateValues();
}

void CMaterialLightPanel::UpdateValues()
{
    char speed[32];
    V_snprintf(speed, sizeof(speed), "%.2fx", sun_speed_.slider->GetValue() / 100.0f);
    sun_speed_.value->SetText(speed);

    for (size_t index = 0; index < lights_.size(); index++)
    {
        char text[32];

        if (index >= 5)
        {
            V_snprintf(text, sizeof(text), "%.3f", lights_[index].slider->GetValue() / 1000.0f);
        }
        else
        {
            V_snprintf(text, sizeof(text), "%d", lights_[index].slider->GetValue());
        }

        lights_[index].value->SetText(text);
    }
}

void CMaterialLightPanel::UpdateAnimatedAngles()
{
    const float angles[] = {state_.get_light_azimuth(), state_.get_light_elevation()};

    for (size_t index = 0; index < std::size(angles); index++)
    {
        if (!lights_[index].slider->IsDragged())
        {
            lights_[index].slider->SetValue(static_cast<int>(std::round(angles[index])), false);
        }
    }

    UpdateValues();
}

void CMaterialLightPanel::OnSliderMoved(KeyValues* params)
{
    Panel* panel = static_cast<Panel*>(params->GetPtr("panel"));
    int value = params->GetInt("position");

    if (panel == sun_speed_.slider)
    {
        state_.SetSunSpeed(value / 100.0f);
        sun_speed_.slider->SetValue(static_cast<int>(std::round(state_.get_sun_speed() * 100.0f)), false);
        UpdateValues();
        return;
    }

    for (size_t index = 0; index < lights_.size(); index++)
    {
        if (panel != lights_[index].slider)
        {
            continue;
        }

        lights_[index].slider->SetValue(value, false);

        if (index < 2)
        {
            state_.SetLightAngles(
                index == 0 ? static_cast<float>(value) : state_.get_light_azimuth(),
                index == 1 ? static_cast<float>(value) : state_.get_light_elevation()
            );
        }
        else if (index < 5)
        {
            std::array<int, 3> color = state_.get_light_color();
            color[index - 2] = value;
            state_.SetLightColor(color[0], color[1], color[2]);
        }
        else
        {
            state_.SetLightStrengths(
                index == 5 ? value / 1000.0f : state_.get_ambient(), index == 6 ? value / 1000.0f : state_.get_directional()
            );
        }

        selected_preset_ = 3;
        preset_->SilentActivateItemByRow(3);
        UpdateValues();
        return;
    }
}

void CMaterialLightPanel::OnTextChanged(KeyValues* params)
{
    if (params->GetPtr("panel") != preset_)
    {
        return;
    }

    for (int preset = 0; preset < 3; preset++)
    {
        wchar_t caption[128];
        preset_->GetItemText(preset_->GetItemIDFromRow(preset), caption, sizeof(caption));

        if (std::wcscmp(params->GetWString("text"), caption) == 0)
        {
            selected_preset_ = preset;
            state_.SetLightPreset(static_cast<MaterialLightPreset>(preset));
            Synchronize();
            return;
        }
    }
}

void CMaterialLightPanel::OnCheckButtonChecked(KeyValues* params)
{
    Panel* panel = static_cast<Panel*>(params->GetPtr("panel"));
    bool checked = params->GetInt("state") != 0;

    if (panel == sun_)
    {
        state_.SetSunGizmo(checked);
        sun_->SilentSetSelected(checked);
    }
    else if (panel == occluder_)
    {
        state_.SetOccluder(checked);
        occluder_->SilentSetSelected(checked);
    }
    else if (panel == animate_sun_)
    {
        state_.SetSunAnimating(checked);
        Synchronize();
    }
}

} // namespace material_editor
