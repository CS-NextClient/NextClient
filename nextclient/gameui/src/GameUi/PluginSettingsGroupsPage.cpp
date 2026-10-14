#include "PluginSettingsGroupsPage.h"

#include <algorithm>
#include <string>

#include <KeyValues.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISchemeNext.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>

#include "PluginLocalization.h"
#include "Controls/SettingsPanelList.h"
#include "PluginSettingsEditor.h"
#include "PluginText.h"

#undef PostMessage

namespace
{
    constexpr int kHeaderHeight = 30;
    constexpr int kEditorInset = 8;
    constexpr int kArrowWidth = 16;
    constexpr wchar_t kDownArrow[] = L"u";
    constexpr wchar_t kRightArrow[] = L"4";
} // namespace

class CPluginSettingsHeader : public vgui2::Button
{
    DECLARE_CLASS_SIMPLE(CPluginSettingsHeader, vgui2::Button);

public:
    CPluginSettingsHeader(vgui2::Panel* parent, const wchar_t* title, vgui2::Panel* target) :
        BaseClass(parent, "Header", "", target, ""),
        arrow_(kRightArrow)
    {
        SetText(title);
        SetContentAlignment(vgui2::Label::a_west);
        SetTextInset(2 * kEditorInset + kArrowWidth, 0);
    }

    void SetExpanded(bool expanded)
    {
        arrow_.SetText(expanded ? kDownArrow : kRightArrow);
        InvalidateLayout();
    }

    void ApplySchemeSettings(vgui2::IScheme* scheme) override
    {
        BaseClass::ApplySchemeSettings(scheme);
        arrow_.SetFont(scheme->GetFont("Marlett", IsProportional()));
    }

    void PerformLayout() override
    {
        arrow_.ResizeImageToContent();
        BaseClass::PerformLayout();
    }

    void Paint() override
    {
        BaseClass::Paint();
        int wide, tall;
        arrow_.GetSize(wide, tall);
        arrow_.SetPos(kEditorInset + (kArrowWidth - wide) / 2, (GetTall() - tall) / 2);
        arrow_.SetColor(GetButtonFgColor());
        arrow_.Paint();
    }

private:
    vgui2::TextImage arrow_;
};

CPluginSettingsGroupsPage::CPluginSettingsGroupsPage(
    vgui2::Panel* parent,
    const tao::json::value& plugins,
    const PluginSettingsSnapshot& current
) :
    BaseClass(parent, "PluginSettingsGroups"),
    state_(plugins),
    refresh_target_(parent)
{
    list_ = new CSettingsPanelList(this, "PluginSections");
    list_->SetFirstColumnWidth(0);
    for (const PluginSettingsGroupsState::Group& group : state_.groups())
    {
        vgui2::EditablePanel* panel = new vgui2::EditablePanel(list_, group.owner.c_str());
        panel->SetPaintBackgroundEnabled(false);
        panel->SetTabPosition(static_cast<int>(sections_.size()) + 1);
        const std::wstring title = PluginWide(PluginMetadataText(group.metadata, "name", PluginLanguage()));
        CPluginSettingsHeader* header = new CPluginSettingsHeader(panel, title.c_str(), this);
        KeyValues* command = new KeyValues("TogglePlugin");
        command->SetString("owner", group.owner.c_str());
        header->SetCommand(command);
        header->SetTabPosition(1);
        CPluginSettingsEditor* editor = new CPluginSettingsEditor(panel, parent, group.controls, current);
        editor->SetTabPosition(2);
        sections_.push_back({panel, header, editor});
    }
    UpdateSections(true);
}

void CPluginSettingsGroupsPage::BeginSession(const PluginSettingsSnapshot& current)
{
    state_.BeginSession(current);
    RefreshAvailability(current);
    UpdateSections(true);
}

void CPluginSettingsGroupsPage::RefreshAvailability(const PluginSettingsSnapshot& current)
{
    const bool changed = state_.RefreshAvailability(current);
    for (const Section& section : sections_)
    {
        section.editor->RefreshAvailability(current);
    }
    UpdateSections(changed);
}

bool CPluginSettingsGroupsPage::has_available_controls() const
{
    return state_.has_available_controls();
}

void CPluginSettingsGroupsPage::UpdateSections(bool rebuild)
{
    if (rebuild)
    {
        list_->RemoveAll();
    }
    for (size_t index = 0; index < sections_.size(); ++index)
    {
        const PluginSettingsGroupsState::Group& group = state_.groups()[index];
        Section& section = sections_[index];
        const bool expanded = group.available && state_.is_expanded(group.owner);
        const vgui2::VPANEL focus = vgui2::input()->GetFocus();
        if (!group.available && focus && vgui2::ipanel()->HasParent(focus, section.panel->GetVPanel()))
        {
            for (size_t next = 0; next < sections_.size(); ++next)
            {
                if (state_.groups()[next].available)
                {
                    sections_[next].header->RequestFocus();
                    break;
                }
            }
        }
        else if (!expanded && focus &&
                 (focus == section.editor->GetVPanel() || vgui2::ipanel()->HasParent(focus, section.editor->GetVPanel())))
        {
            section.header->RequestFocus();
        }
        section.editor->SetVisible(expanded);
        section.editor->SetKeyBoardInputEnabled(expanded);
        section.header->SetExpanded(expanded);
        section.panel->SetTall(kHeaderHeight + (expanded ? section.editor->GetTall() + kEditorInset : 0));
        section.panel->SetVisible(group.available);
        if (rebuild)
        {
            section.row = group.available ? list_->GetItemCount() : -1;
            if (group.available)
            {
                list_->AddItem(nullptr, section.panel);
            }
        }
    }
    list_->InvalidateLayout(true);
    InvalidateLayout();
}

void CPluginSettingsGroupsPage::PerformLayout()
{
    BaseClass::PerformLayout();
    list_->SetBounds(4, 4, GetWide() - 8, GetTall() - 8);
    list_->InvalidateLayout(true);
    for (const Section& section : sections_)
    {
        section.header->SetBounds(0, 0, section.panel->GetWide(), kHeaderHeight);
        section.editor->SetBounds(
            kEditorInset, kHeaderHeight + kEditorInset, std::max(0, section.panel->GetWide() - 2 * kEditorInset), section.editor->GetTall()
        );
    }
}

void CPluginSettingsGroupsPage::OnToggle(const char* owner)
{
    state_.Toggle(owner);
    UpdateSections(false);
    for (size_t index = 0; index < sections_.size(); ++index)
    {
        if (state_.is_expanded(owner) && state_.groups()[index].owner == owner)
        {
            list_->ScrollToItem(sections_[index].row);
            break;
        }
    }
}

void CPluginSettingsGroupsPage::OnPageShow()
{
    BaseClass::OnPageShow();
    PostMessage(refresh_target_, new KeyValues("PluginSettingsRefresh"));
}

void CPluginSettingsGroupsPage::OnResetData()
{
    for (const Section& section : sections_)
    {
        section.editor->ResetControls();
    }
}

void CPluginSettingsGroupsPage::OnApplyChanges()
{
    OnResetData();
}

void CPluginSettingsGroupsPage::Collect(tao::json::value& values, const PluginSettingsSnapshot& current)
{
    for (const Section& section : sections_)
    {
        section.editor->Collect(values, current);
    }
}
