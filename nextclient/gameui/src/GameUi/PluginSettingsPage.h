#pragma once
#include "PluginSettingsState.h"
#include <vgui_controls/PropertyPage.h>
#include <tao/json.hpp>
#include <string>
#include <vector>
namespace vgui2
{
    class PanelListPanel;
    class PropertySheet;
} // namespace vgui2
class CPluginSettingsPage : public vgui2::PropertyPage
{
    DECLARE_CLASS_SIMPLE(CPluginSettingsPage, vgui2::PropertyPage);

public:
    CPluginSettingsPage(
        vgui2::Panel* parent,
        const tao::json::value& controls,
        const PluginSettingsSnapshot& current,
        vgui2::PropertyPage* original = nullptr
    );
    void PerformLayout() override;
    void OnCommand(const char*) override;
    void Collect(tao::json::value& values, const PluginSettingsSnapshot& current);
    void OnPageShow() override;
    void OnPageHide() override;
    MESSAGE_FUNC(OnResetData, "ResetData");
    MESSAGE_FUNC(OnApplyChanges, "ApplyChanges");
    MESSAGE_FUNC(OnModified, "ApplyButtonEnable");
    MESSAGE_FUNC(OnCheck, "CheckButtonChecked");
    MESSAGE_FUNC(OnSlider, "SliderMoved");
    MESSAGE_FUNC(OnText, "TextChanged");

private:
    struct Control
    {
        tao::json::value spec;
        vgui2::Panel* widget;
    };
    PluginSettingsState state_;
    const PluginSettingsSnapshot& current_;
    void ResetControls();
    std::vector<Control> controls_;
    vgui2::PanelListPanel* list_;
    vgui2::PropertySheet* sheet_{};
    void ForwardPageEvent(const char* event);
};
