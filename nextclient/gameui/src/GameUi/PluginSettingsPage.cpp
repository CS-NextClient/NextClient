#include "PluginSettingsPage.h"
#include "Controls/WrappedLabel.h"
#include "PluginLocalization.h"
#include <nextclient/runtime.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui/IPanel.h>
#include <KeyValues.h>
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

CPluginSettingsPage::CPluginSettingsPage(vgui2::Panel* parent, const tao::json::value& controls, vgui2::PropertyPage* original) :
    BaseClass(parent, "PluginSettings")
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
        vgui2::Panel* widget = nullptr;
        auto kind = spec.at("kind").as<unsigned>();
        auto name = spec.at("id").get_string();
        if (kind == NC_CHECKBOX)
        {
            auto* checkbox = new vgui2::CheckButton(list_, name.c_str(), "");
            checkbox->SetText(caption.c_str());
            widget = checkbox;
        }
        else if (kind == NC_SLIDER)
        {
            auto* slider = new vgui2::Slider(list_, name.c_str());
            slider->SetRange(spec.at("min").as<int>(), spec.at("max").as<int>());
            widget = slider;
        }
        else if (kind == NC_CHOICE)
        {
            auto* combo = new vgui2::ComboBox(list_, name.c_str(), 8, false);
            auto choices = PluginLocalized(spec, "choices_en", "choices_ru");
            size_t start = 0;
            do
            {
                auto end = choices.find('\n', start);
                combo->AddItem(PluginWide(choices.substr(start, end == std::string::npos ? end : end - start)).c_str(), nullptr);
                if (end == std::string::npos)
                    break;
                start = end + 1;
            } while (start <= choices.size());
            widget = combo;
        }
        else
        {
            auto* button =
                new vgui2::Button(list_, name.c_str(), "#NextPlugins_Run", this, ("Action" + std::to_string(controls_.size())).c_str());
            widget = button;
        }
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
    auto current = tao::json::from_string(nc_runtime_ui());
    for (auto& c : controls_)
    {
        state_.ResetControl(c.spec, current);
        int value = c.spec.at("value").as<int>();
        const auto* active = FindPluginControl(current, c.spec);
        c.widget->SetEnabled(active != nullptr);
        if (active)
            value = active->at("value").as<int>();
        switch (c.spec.at("kind").as<unsigned>())
        {
            case NC_CHECKBOX:
                static_cast<vgui2::CheckButton*>(c.widget)->SetSelected(value != 0);
                break;
            case NC_SLIDER:
                static_cast<vgui2::Slider*>(c.widget)->SetValue(value);
                break;
            case NC_CHOICE:
                static_cast<vgui2::ComboBox*>(c.widget)->ActivateItemByRow(value);
                break;
        }
    }
}
void CPluginSettingsPage::Collect(tao::json::value& values, const tao::json::value& current)
{
    for (const auto& c : controls_)
    {
        const bool available = FindPluginControl(current, c.spec) != nullptr;
        c.widget->SetEnabled(available);
        if (!available)
            continue;
        int value = 0;
        switch (c.spec.at("kind").as<unsigned>())
        {
            case NC_CHECKBOX:
                value = static_cast<vgui2::CheckButton*>(c.widget)->IsSelected() ? 1 : 0;
                break;
            case NC_SLIDER:
                value = static_cast<vgui2::Slider*>(c.widget)->GetValue();
                break;
            case NC_CHOICE:
                value = static_cast<vgui2::ComboBox*>(c.widget)->GetActiveItem();
                break;
            default:
                continue;
        }
        state_.Collect(values, c.spec, value, current);
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
