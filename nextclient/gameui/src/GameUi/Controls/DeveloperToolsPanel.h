#pragma once

#include <array>

#include <vgui_controls/Panel.h>
#include <goldsrc/client_api.h>

namespace vgui2
{
    class ILocalize;
    class Button;
    class Label;
} // namespace vgui2

class CDeveloperToolsPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CDeveloperToolsPanel, vgui2::Panel);

private:
    goldsrc::cl_enginefunc_t& engine_;
    vgui2::ILocalize& localize_;
    vgui2::Label* indicator_{};
    vgui2::Button* editor_{};
    std::array<vgui2::Button*, 4> monitors_{};
    std::array<int, 4> modes_{};
    float developer_value_{};
    bool refresh_text_ = true;

public:
    CDeveloperToolsPanel(vgui2::Panel* parent, goldsrc::cl_enginefunc_t& engine, vgui2::ILocalize& localize);
    void Refresh(bool menu_available, bool authoring_available);
    void PerformLayout() override;
    void ApplySchemeSettings(vgui2::IScheme* scheme) override;
    void OnCommand(const char* command) override;
};
