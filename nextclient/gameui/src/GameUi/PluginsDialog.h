#pragma once
#include <vgui_controls/Frame.h>
#include <tao/json.hpp>
#include "PluginsDialogState.h"

namespace vgui2
{
    class ListPanel;
    class TextEntry;
    class Label;
    class Button;
    class QueryBox;
} // namespace vgui2
class CPluginsDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CPluginsDialog, vgui2::Frame);

public:
    explicit CPluginsDialog(vgui2::Panel* parent);
    void Activate() override;
    void OnCommand(const char*) override;
    void PerformLayout() override;
    MESSAGE_FUNC(OnItemSelected, "ItemSelected");
    MESSAGE_FUNC_PARAMS(OnPermissionAccepted, "PluginPermissionAccepted", data);

private:
    int Selected() const;
    std::string SelectionJson() const;
    std::vector<PluginSelection> Selection() const;
    void Refresh(int selected = -1);
    tao::json::value rows_;
    std::vector<PluginSelection> initialSelection_;
    bool safeMode_{};
    std::string diagnostic_;
    vgui2::ListPanel* list_;
    vgui2::TextEntry* details_;
    vgui2::Label* notice_;
    vgui2::Label* changes_;
    vgui2::Button *toggle_, *up_, *down_, *recommended_, *ok_, *cancel_;
    vgui2::DHANDLE<vgui2::QueryBox> permissionDialog_;
    int consentRequest_{};
};
