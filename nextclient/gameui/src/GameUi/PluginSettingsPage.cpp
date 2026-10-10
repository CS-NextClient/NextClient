#include "PluginSettingsPage.h"

#include <KeyValues.h>
#include <vgui/IPanel.h>
#include <vgui_controls/PropertySheet.h>

#include "Controls/SettingsPanelList.h"
#include "PluginSettingsEditor.h"

#undef PostMessage
#undef SendMessage

CPluginSettingsPage::CPluginSettingsPage(
    vgui2::Panel* parent,
    const tao::json::value& controls,
    const PluginSettingsSnapshot& current,
    vgui2::PropertyPage* original
) :
    BaseClass(parent, "PluginSettings")
{
    list_ = new CSettingsPanelList(this, "Controls");
    list_->SetFirstColumnWidth(0);
    editor_ = new CPluginSettingsEditor(list_, parent, controls, current);
    editor_->SetTabPosition(1);
    list_->AddItem(nullptr, editor_);
    if (original)
    {
        sheet_ = new vgui2::PropertySheet(this, "Sections");
        sheet_->SetTabPosition(1);
        sheet_->AddPage(original, "#NextPlugins_Standard");
        sheet_->AddPage(list_, "#NextPlugins_Title");
        sheet_->AddActionSignalTarget(this);
    }
    OnResetData();
}

void CPluginSettingsPage::PerformLayout()
{
    BaseClass::PerformLayout();
    if (sheet_)
    {
        sheet_->SetBounds(0, 0, GetWide(), GetTall());
    }
    else
    {
        list_->SetBounds(4, 4, GetWide() - 8, GetTall() - 8);
    }
}

void CPluginSettingsPage::OnResetData()
{
    if (sheet_)
    {
        sheet_->ResetAllData();
    }
    editor_->ResetControls();
}

void CPluginSettingsPage::RefreshAvailability(const PluginSettingsSnapshot& current)
{
    editor_->RefreshAvailability(current);
}

void CPluginSettingsPage::Collect(tao::json::value& values, const PluginSettingsSnapshot& current)
{
    editor_->Collect(values, current);
}

void CPluginSettingsPage::ForwardPageEvent(const char* event)
{
    if (sheet_)
    {
        if (vgui2::Panel* active = sheet_->GetActivePage())
        {
            KeyValues::AutoDelete message(event);
            vgui2::ipanel()->SendMessage(active->GetVPanel(), message, GetVPanel());
        }
    }
}
void CPluginSettingsPage::OnPageShow()
{
    BaseClass::OnPageShow();
    ForwardPageEvent("PageShow");
}
void CPluginSettingsPage::OnPageHide()
{
    ForwardPageEvent("PageHide");
    BaseClass::OnPageHide();
}
void CPluginSettingsPage::OnApplyChanges()
{
    if (sheet_)
    {
        sheet_->ApplyChanges();
    }
    editor_->ResetControls();
}

void CPluginSettingsPage::OnModified()
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));
}
