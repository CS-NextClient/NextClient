#include "PluginSettingsEditor.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <KeyValues.h>
#include <nextclient/runtime.h>
#include <vgui_controls/Label.h>

#include "Controls/WrappedLabel.h"
#include "PluginControls.h"
#include "PluginLocalization.h"

namespace
{
    class CPluginSettingRow : public vgui2::EditablePanel
    {
    public:
        CPluginSettingRow(vgui2::Panel* parent, vgui2::Label* label, vgui2::Panel* widget) :
            vgui2::EditablePanel(parent, "SettingRow"),
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

CPluginSettingsEditor::CPluginSettingsEditor(
    vgui2::Panel* parent,
    vgui2::Panel* target,
    const tao::json::value& controls,
    const PluginSettingsSnapshot& current
) :
    BaseClass(parent, "PluginSettingsEditor"),
    current_(current)
{
    SetPaintBackgroundEnabled(false);
    AddActionSignalTarget(target);
    int height = 0;
    for (const auto& spec : controls.get_array())
    {
        const unsigned kind = spec.at("kind").as<unsigned>();
        const std::string command = "Action" + std::to_string(controls_.size());
        vgui2::Panel* widget = PluginControls_Create(this, PluginControls_SettingsSpec(spec), this, command.c_str());
        widget->AddActionSignalTarget(this);
        vgui2::Panel* row = widget;
        if (kind == NC_CHECKBOX)
        {
            row->SetTall(28);
        }
        else
        {
            CWrappedLabel* label = new CWrappedLabel(this, "Label", PluginWide(PluginLocalized(spec)).c_str());
            label->SetWrap(true);
            row = new CPluginSettingRow(this, label, widget);
            widget->SetTabPosition(1);
        }
        row->SetTabPosition(static_cast<int>(controls_.size()) + 1);
        height += row->GetTall() + 5;
        controls_.push_back({spec, widget, row});
    }
    SetTall(height);
    ResetControls();
}

void CPluginSettingsEditor::PerformLayout()
{
    BaseClass::PerformLayout();
    int y = 0;
    for (const Control& control : controls_)
    {
        control.row->SetBounds(0, y, GetWide(), control.row->GetTall());
        y += control.row->GetTall() + 5;
    }
}

void CPluginSettingsEditor::RefreshAvailability(const PluginSettingsSnapshot& current)
{
    for (const Control& control : controls_)
    {
        control.widget->SetEnabled(current.Find(control.spec) != nullptr);
    }
}

void CPluginSettingsEditor::ResetControls()
{
    for (auto& c : controls_)
    {
        const auto* active = state_.ResetControl(c.spec, current_);
        int value = c.spec.at("value").as<int>();
        c.widget->SetEnabled(active != nullptr);
        if (active)
        {
            value = active->at("value").as<int>();
        }
        PluginControls_SetValue(c.widget, PluginControls_SettingKind(c.spec.at("kind").as<unsigned>()), value);
    }
}
void CPluginSettingsEditor::Collect(tao::json::value& values, const PluginSettingsSnapshot& current)
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
void CPluginSettingsEditor::OnModified()
{
    PostActionSignal(new KeyValues("ApplyButtonEnable"));
}
void CPluginSettingsEditor::OnCheck()
{
    OnModified();
}
void CPluginSettingsEditor::OnSlider()
{
    OnModified();
}
void CPluginSettingsEditor::OnText()
{
    OnModified();
}
void CPluginSettingsEditor::OnCommand(const char* command)
{
    if (!strncmp(command, "Action", 6))
    {
        size_t n = static_cast<size_t>(atoi(command + 6));
        if (n < controls_.size())
        {
            nc_runtime_action(controls_[n].spec.at("owner").get_string().c_str(), controls_[n].spec.at("id").get_string().c_str());
            PostActionSignal(new KeyValues("PluginSettingsRefresh"));
        }
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}
