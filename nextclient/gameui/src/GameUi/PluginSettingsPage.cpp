#include "PluginSettingsPage.h"

#include <nextclient/runtime.h>
#include <vgui/IPanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/PropertySheet.h>
#include <KeyValues.h>

#include "Controls/WrappedLabel.h"
#include "PluginControls.h"
#include "PluginLocalization.h"

#undef PostMessage
#undef SendMessage

namespace
{
    class CPluginSettingRow : public vgui2::Panel
    {
    public:
        CPluginSettingRow(vgui2::Panel* parent, vgui2::Label* label, vgui2::Panel* widget) :
            vgui2::Panel(parent, "SettingRow"),
            label_(label),
            widget_(widget)
        {
            SetTall(36);
            SetPaintBackgroundEnabled(false);
            label_->SetParent(this);
            widget_->SetParent(this);
        }

        void PerformLayout() override
        {
            int split = GetWide() / 2;
            label_->SetBounds(0, 0, split - 8, GetTall());
            widget_->SetBounds(split, (GetTall() - 28) / 2, GetWide() - split, 28);
        }

    private:
        vgui2::Label* label_;
        vgui2::Panel* widget_;
    };
} // namespace

CPluginSettingsPage::CPluginSettingsPage(
    vgui2::Panel* parent,
    const tao::json::value& controls,
    const PluginSettingsSnapshot& current,
    vgui2::PropertyPage* original
) :
    BaseClass(parent, "PluginSettings"),
    current_(current)
{
    list_ = new vgui2::PanelListPanel(this, "Controls");
    list_->SetFirstColumnWidth(0);
    if (original)
    {
        sheet_ = new vgui2::PropertySheet(this, "Sections");
        sheet_->AddPage(original, "#NextPlugins_Standard");
        sheet_->AddPage(list_, "#NextPlugins_Title");
        sheet_->AddActionSignalTarget(this);
    }
    for (const auto& spec : controls.get_array())
    {
        auto caption = PluginWide(PluginLocalized(spec));
        const unsigned kind = spec.at("kind").as<unsigned>();
        const std::string command = "Action" + std::to_string(controls_.size());
        vgui2::Panel* widget = PluginControls_Create(list_, PluginControls_SettingsSpec(spec), this, command.c_str());
        widget->SetTall(28);
        widget->AddActionSignalTarget(this);
        if (kind == NC_CHECKBOX)
            list_->AddItem(nullptr, widget);
        else
        {
            auto* label = new CWrappedLabel(list_, "Label", caption.c_str());
            label->SetWrap(true);
            list_->AddItem(nullptr, new CPluginSettingRow(list_, label, widget));
        }
        controls_.push_back({spec, widget});
    }
    OnResetData();
}
void CPluginSettingsPage::PerformLayout()
{
    BaseClass::PerformLayout();
    if (sheet_)
        sheet_->SetBounds(0, 0, GetWide(), GetTall());
    else
        list_->SetBounds(4, 4, GetWide() - 8, GetTall() - 8);
}
void CPluginSettingsPage::OnResetData()
{
    if (sheet_)
        sheet_->ResetAllData();
    ResetControls();
}
void CPluginSettingsPage::ResetControls()
{
    for (auto& c : controls_)
    {
        const auto* active = state_.ResetControl(c.spec, current_);
        int value = c.spec.at("value").as<int>();
        c.widget->SetEnabled(active != nullptr);
        if (active)
            value = active->at("value").as<int>();
        PluginControls_SetValue(c.widget, PluginControls_SettingKind(c.spec.at("kind").as<unsigned>()), value);
    }
}
void CPluginSettingsPage::Collect(tao::json::value& values, const PluginSettingsSnapshot& current)
{
    for (const auto& c : controls_)
    {
        const auto value = PluginControls_Read(c.widget, PluginControls_SettingKind(c.spec.at("kind").as<unsigned>()));
        if (!value)
        {
            c.widget->SetEnabled(current.Find(c.spec) != nullptr);
            continue;
        }
        const int setting = value->is_boolean() ? (value->get_boolean() ? 1 : 0) : value->as<int>();
        c.widget->SetEnabled(state_.Collect(values, c.spec, setting, current));
    }
}
void CPluginSettingsPage::ForwardPageEvent(const char* event)
{
    if (sheet_)
        if (auto* active = sheet_->GetActivePage())
        {
            KeyValues::AutoDelete message(event);
            vgui2::ipanel()->SendMessage(active->GetVPanel(), message, GetVPanel());
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
        sheet_->ApplyChanges();
    ResetControls();
}
void CPluginSettingsPage::OnModified()
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));
}
void CPluginSettingsPage::OnCheck()
{
    OnModified();
}
void CPluginSettingsPage::OnSlider()
{
    OnModified();
}
void CPluginSettingsPage::OnText()
{
    OnModified();
}
void CPluginSettingsPage::OnCommand(const char* command)
{
    if (!strncmp(command, "Action", 6))
    {
        size_t n = static_cast<size_t>(atoi(command + 6));
        if (n < controls_.size())
            nc_runtime_action(controls_[n].spec.at("owner").get_string().c_str(), controls_[n].spec.at("id").get_string().c_str());
    }
    else
        BaseClass::OnCommand(command);
}
