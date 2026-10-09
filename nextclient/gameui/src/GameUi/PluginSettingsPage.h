#pragma once

#include <vgui_controls/PropertyPage.h>

#include "PluginSettingsState.h"

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
    void Collect(tao::json::value& values, const PluginSettingsSnapshot& current);
    void RefreshAvailability(const PluginSettingsSnapshot& current);
    void OnPageShow() override;
    void OnPageHide() override;
    MESSAGE_FUNC(OnResetData, "ResetData");
    MESSAGE_FUNC(OnApplyChanges, "ApplyChanges");
    MESSAGE_FUNC(OnModified, "ApplyButtonEnable");

private:
    void ForwardPageEvent(const char* event);
    class CPluginSettingsEditor* editor_;
    vgui2::PanelListPanel* list_;
    vgui2::PropertySheet* sheet_{};
};
