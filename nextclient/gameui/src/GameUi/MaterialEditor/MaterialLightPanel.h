#pragma once

#include <array>

#include <vgui_controls/Panel.h>

#include "GameUi/MaterialEditor/MaterialPreviewState.h"

namespace vgui2
{
    class CheckButton;
    class ComboBox;
    class Label;
    class Slider;
} // namespace vgui2

namespace material_editor
{
class CMaterialLightPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CMaterialLightPanel, vgui2::Panel);

private:
    struct LightControl
    {
        vgui2::Label* label{};
        vgui2::Slider* slider{};
        vgui2::Label* value{};
    };

    MaterialPreviewState& state_;
    std::array<LightControl, 7> lights_{};
    LightControl sun_speed_{};
    vgui2::ComboBox* preset_{};
    vgui2::CheckButton* sun_{};
    vgui2::CheckButton* occluder_{};
    vgui2::CheckButton* animate_sun_{};
    int selected_preset_{};

public:
    CMaterialLightPanel(vgui2::Panel* parent, MaterialPreviewState& state);
    void PerformLayout() override;
    void Synchronize();
    void UpdateAnimatedAngles();

private:
    MESSAGE_FUNC_PARAMS(OnSliderMoved, "SliderMoved", params);
    MESSAGE_FUNC_PARAMS(OnTextChanged, "TextChanged", params);
    MESSAGE_FUNC_PARAMS(OnCheckButtonChecked, "CheckButtonChecked", params);
    void UpdateValues();
};

} // namespace material_editor
