#pragma once

#include <vector>

#include <vgui_controls/EditablePanel.h>

#include "PluginSettingsState.h"

class CPluginSettingsEditor : public vgui2::EditablePanel
{
    DECLARE_CLASS_SIMPLE(CPluginSettingsEditor, vgui2::EditablePanel);

public:
    CPluginSettingsEditor(
        vgui2::Panel* parent,
        vgui2::Panel* target,
        const tao::json::value& controls,
        const PluginSettingsSnapshot& current
    );
    void PerformLayout() override;
    void OnCommand(const char* command) override;
    void ResetControls();
    void RefreshAvailability(const PluginSettingsSnapshot& current);
    void Collect(tao::json::value& values, const PluginSettingsSnapshot& current);
    MESSAGE_FUNC(OnModified, "ApplyButtonEnable");
    MESSAGE_FUNC(OnCheck, "CheckButtonChecked");
    MESSAGE_FUNC(OnSlider, "SliderMoved");
    MESSAGE_FUNC(OnText, "TextChanged");

private:
    struct Control
    {
        tao::json::value spec;
        vgui2::Panel* widget;
        vgui2::Panel* row;
    };
    PluginSettingsState state_;
    const PluginSettingsSnapshot& current_;
    std::vector<Control> controls_;
};
