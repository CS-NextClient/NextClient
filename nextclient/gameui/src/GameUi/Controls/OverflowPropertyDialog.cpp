#include "OverflowPropertyDialog.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>

#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>
#include <vgui_controls/PropertySheet.h>

#include "Controls/PropertyPageNavigation.h"
#include "Controls/PropertyPagePicker.h"

namespace
{
    class CPagePicker : public PropertyPagePicker<vgui2::ComboBox>
    {
    public:
        explicit CPagePicker(vgui2::Panel* parent) :
            PropertyPagePicker<vgui2::ComboBox>(parent, "AllSettingsTabs", 12, false)
        {
            using page_menu_t = PropertyPagePickerMenu<vgui2::Menu>;
            page_menu_t* menu = new page_menu_t(this, "Pages");
            SetMenu(menu);
            menu->AddActionSignalTarget(this);
            menu->SetTypeAheadMode(vgui2::Menu::COMPAT_MODE);
            SetNumberOfEditLines(12);
        }
    };
} // namespace

class CEnabledPropertySheet : public vgui2::PropertySheet
{
    DECLARE_CLASS_SIMPLE(CEnabledPropertySheet, vgui2::PropertySheet);

public:
    explicit CEnabledPropertySheet(vgui2::Panel* parent) :
        BaseClass(parent, "SettingsSheet")
    {}

    CEnabledPropertySheet(vgui2::Panel* parent, CPagePicker* picker) :
        BaseClass(parent, "PickerSheet", picker),
        picker_(picker)
    {
        ShowContextButtons(false);
    }

    void AddPage(vgui2::Panel* page, const char* title, const char* image = nullptr, bool context_menu = false) override
    {
        navigation_.AddPage(page);
        BaseClass::AddPage(page, title, image, context_menu);
        // VGUI exposes only the active tab. Bind each newly created tab to its
        // page once, independently of translated or duplicate captions.
        vgui2::Panel* tab = nullptr;
        for (int index = 0; index < GetChildCount(); ++index)
        {
            vgui2::Panel* child = GetChild(index);
            if (!strcmp(child->GetName(), "tab") &&
                std::none_of(tabs_.begin(), tabs_.end(), [child](const auto& entry) { return entry.second == child; }))
            {
                tab = child;
                break;
            }
        }
        assert(tab);
        tabs_.emplace(page, tab);
    }

    void RemovePage(vgui2::Panel* page) override
    {
        navigation_.RemovePage(page);
        tabs_.erase(page);
        BaseClass::RemovePage(page);
        page->RemoveActionSignalTarget(this);
    }

    void SetPageEnabled(vgui2::Panel* page, bool enabled)
    {
        navigation_.SetPageEnabled(page, enabled);
        tabs_.at(page)->SetEnabled(enabled);
        if (picker_)
        {
            picker_->SetItemEnabled(picker_->GetItemIDFromRow(FindPage(page)), enabled);
        }
        if (!enabled && GetActivePage() == page)
        {
            const int first = navigation_.FindEnabled(0);
            if (first >= 0)
            {
                SetActivePage(GetPage(first));
            }
        }
    }

    vgui2::Panel* get_page_tab(vgui2::Panel* page) const
    {
        return tabs_.at(page);
    }

protected:
    void ChangeActiveTab(int index) override
    {
        if (GetNumPages() == 0)
        {
            BaseClass::ChangeActiveTab(index);
            return;
        }
        index = (index % GetNumPages() + GetNumPages()) % GetNumPages();
        if (navigation_.is_page_enabled(GetPage(index)))
        {
            BaseClass::ChangeActiveTab(index);
            if (picker_)
            {
                picker_->SetSelection(GetActivePageNum());
            }
        }
    }

    void OnKeyCodePressed(vgui2::KeyCode code) override
    {
        if (IsKBNavigationEnabled() && GetActiveTab() && GetActiveTab()->HasFocus())
        {
            int direction = 0;
            switch (code)
            {
                case vgui2::KEY_RIGHT:
                case vgui2::KEY_XBUTTON_RIGHT:
                case vgui2::KEY_XSTICK1_RIGHT:
                case vgui2::KEY_XSTICK2_RIGHT:
                    direction = 1;
                    break;
                case vgui2::KEY_LEFT:
                case vgui2::KEY_XBUTTON_LEFT:
                case vgui2::KEY_XSTICK1_LEFT:
                case vgui2::KEY_XSTICK2_LEFT:
                    direction = -1;
                    break;
                default:
                    break;
            }
            if (direction)
            {
                const int next = navigation_.FindEnabled(GetActivePageNum() + direction, direction);
                if (next >= 0)
                {
                    ChangeActiveTab(next);
                }
                return;
            }
        }
        BaseClass::OnKeyCodePressed(code);
    }

private:
    MESSAGE_FUNC(OnPickerTextChanged, "TextChanged") {}

    MESSAGE_FUNC_INT(OnPagePicked, "PagePicked", row)
    {
        if (row >= 0 && row < GetNumPages() && row != GetActivePageNum())
        {
            SetActivePage(GetPage(row));
        }
        picker_->SetSelection(GetActivePageNum());
    }
    PropertyPageNavigation navigation_;
    std::map<vgui2::Panel*, vgui2::Panel*> tabs_;
    CPagePicker* picker_{};
};

COverflowPropertyDialog::COverflowPropertyDialog(vgui2::Panel* parent, const char* name) :
    BaseClass(parent, name)
{
    BaseClass::GetPropertySheet()->SetVisible(false);
    sheet_ = new CEnabledPropertySheet(this);
    sheet_->AddActionSignalTarget(this);
    sheet_->SetTabPosition(1);
}

void COverflowPropertyDialog::AddPage(vgui2::Panel* page, const char* title)
{
    pages_.push_back({page, title});
    sheet_->AddPage(page, title);
}

void COverflowPropertyDialog::SetPageEnabled(vgui2::Panel* page, bool enabled)
{
    for (Page& entry : pages_)
    {
        if (entry.panel == page)
        {
            entry.enabled = enabled;
            sheet_->SetPageEnabled(page, enabled);
            return;
        }
    }
}

vgui2::PropertySheet* COverflowPropertyDialog::GetPropertySheet()
{
    return sheet_;
}

vgui2::Panel* COverflowPropertyDialog::GetActivePage()
{
    return sheet_->GetActivePage();
}

void COverflowPropertyDialog::ResetAllData()
{
    sheet_->ResetAllData();
}

void COverflowPropertyDialog::RequestFocus(int direction)
{
    sheet_->RequestFocus(direction);
}

void COverflowPropertyDialog::FitPageNavigation()
{
    int screen_wide, screen_tall;
    vgui2::surface()->GetScreenSize(screen_wide, screen_tall);
    const int available = std::max(1, screen_wide - 40);
    if (!picker_ && !pages_.empty())
    {
        sheet_->InvalidateLayout(true);
        vgui2::Panel* last_tab = sheet_->get_page_tab(pages_.back().panel);
        const int required = last_tab->GetXPos() + last_tab->GetWide() + GetWide() - sheet_->GetWide();
        if (required <= available)
        {
            SetWide(std::max(GetWide(), required));
        }
        else
        {
            vgui2::Panel* active = sheet_->GetActivePage();
            CEnabledPropertySheet* previous = sheet_;
            previous->RemoveAllPages();
            previous->SetVisible(false);
            previous->MarkForDeletion();
            CPagePicker* picker = new CPagePicker(this);
            picker_ = picker;
            sheet_ = new CEnabledPropertySheet(this, picker);
            sheet_->SetKBNavigationEnabled(true);
            sheet_->AddActionSignalTarget(this);
            picker_->SetTabPosition(1);
            sheet_->SetTabPosition(2);
            for (const Page& page : pages_)
            {
                sheet_->AddPage(page.panel, page.title.c_str());
                sheet_->SetPageEnabled(page.panel, page.enabled);
            }
            sheet_->SetActivePage(active);
        }
    }
    SetWide(std::min(GetWide(), available));
    InvalidateLayout();
}

void COverflowPropertyDialog::PerformLayout()
{
    BaseClass::PerformLayout();
    int x, y, wide, tall;
    BaseClass::GetPropertySheet()->GetBounds(x, y, wide, tall);
    if (picker_)
    {
        picker_->SetBounds(x, y, wide, 24);
        sheet_->SetBounds(x, y + 28, wide, std::max(0, tall - 28));
    }
    else
    {
        sheet_->SetBounds(x, y, wide, tall);
    }
}

bool COverflowPropertyDialog::OnOK(bool apply_only)
{
    sheet_->ApplyChanges();
    return BaseClass::OnOK(apply_only);
}

void COverflowPropertyDialog::OnCommand(const char* command)
{
    // PropertyDialog disables Apply even when OnOK reports a save failure.
    if (!V_stricmp(command, "Apply") || !V_stricmp(command, "OK"))
    {
        const bool apply_only = !V_stricmp(command, "Apply");
        if (OnOK(apply_only))
        {
            EnableApplyButton(false);
            if (!apply_only)
            {
                BaseClass::OnCommand("Close");
            }
        }
        return;
    }
    BaseClass::OnCommand(command);
}
