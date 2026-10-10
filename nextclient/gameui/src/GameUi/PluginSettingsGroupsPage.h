#pragma once

#include <vector>

#include <vgui_controls/PropertyPage.h>

#include "PluginSettingsGroupsState.h"

namespace vgui2
{
    class PanelListPanel;
} // namespace vgui2

class CPluginSettingsGroupsPage : public vgui2::PropertyPage
{
    DECLARE_CLASS_SIMPLE(CPluginSettingsGroupsPage, vgui2::PropertyPage);

public:
    CPluginSettingsGroupsPage(vgui2::Panel* parent, const tao::json::value& plugins, const PluginSettingsSnapshot& current);
    void BeginSession(const PluginSettingsSnapshot& current);
    void RefreshAvailability(const PluginSettingsSnapshot& current);
    void Collect(tao::json::value& values, const PluginSettingsSnapshot& current);
    bool has_available_controls() const;
    void PerformLayout() override;
    void OnPageShow() override;
    MESSAGE_FUNC(OnResetData, "ResetData");
    MESSAGE_FUNC(OnApplyChanges, "ApplyChanges");
    MESSAGE_FUNC_CHARPTR(OnToggle, "TogglePlugin", owner);

private:
    struct Section
    {
        vgui2::Panel* panel;
        class CPluginSettingsHeader* header;
        class CPluginSettingsEditor* editor;
        int row = -1;
    };
    void UpdateSections(bool rebuild);
    PluginSettingsGroupsState state_;
    std::vector<Section> sections_;
    vgui2::PanelListPanel* list_;
    vgui2::Panel* refresh_target_;
};
