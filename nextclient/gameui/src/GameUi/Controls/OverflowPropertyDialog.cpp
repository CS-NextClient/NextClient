#include "OverflowPropertyDialog.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <KeyValues.h>
#include <tier1/strtools.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/MenuItem.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/Tooltip.h>

#include "Controls/PropertyPageNavigation.h"
#include "Controls/PropertyPageOverflow.h"

namespace
{
    constexpr int kMenuVisibleItems = 12;
    constexpr int kMoreTextInset = 6;
    constexpr int kMoreArrowWidth = 16;
    constexpr int kInactiveTabY = 4;
    constexpr char kMoreTitle[] = "#GameUI_MoreSettingsTabs";
    constexpr wchar_t kDownArrow[] = L"u";

    using page_menu_t = PropertyPageOverflowMenu<vgui2::Menu>;

    class CPageOverflowButton : public PropertyPageOverflowButton<vgui2::MenuButton>
    {
        DECLARE_CLASS_SIMPLE(CPageOverflowButton, vgui2::MenuButton);

    public:
        explicit CPageOverflowButton(vgui2::Panel* parent) :
            PropertyPageOverflowButton<vgui2::MenuButton>(parent, "MoreSettingsTabs", kMoreTitle),
            arrow_(kDownArrow)
        {
            SetContentAlignment(vgui2::Label::a_west);
            SetTextInset(kMoreTextInset, 0);
            SetTabPosition(1);
            SetVisible(false);
        }

        int CalculateWidth()
        {
            int wide, tall;
            GetTextImage()->GetContentSize(wide, tall);
            return wide + 2 * kMoreTextInset + kMoreArrowWidth;
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
            GetTextImage()->SetDrawWidth(std::max(0, GetWide() - 2 * kMoreTextInset - kMoreArrowWidth));
        }

        void Paint() override
        {
            BaseClass::Paint();
            int wide, tall;
            arrow_.GetSize(wide, tall);
            arrow_.SetPos(GetWide() - kMoreTextInset - (kMoreArrowWidth + wide) / 2, (GetTall() - tall) / 2);
            arrow_.SetColor(GetButtonFgColor());
            arrow_.Paint();
        }

        void DrawFocusBorder(int x0, int y0, int x1, int y1) override
        {
            vgui2::Button::DrawFocusBorder(x0, y0, x1, y1);
        }

        void OnShowMenu(vgui2::Menu* menu) override
        {
            static_cast<page_menu_t*>(menu)->HighlightFirstEnabled();
        }

    private:
        vgui2::TextImage arrow_;
    };
} // namespace

class CEnabledPropertySheet : public vgui2::PropertySheet
{
    DECLARE_CLASS_SIMPLE(CEnabledPropertySheet, vgui2::PropertySheet);

public:
    explicit CEnabledPropertySheet(vgui2::Panel* parent) :
        BaseClass(parent, "SettingsSheet")
    {
        more_ = new CPageOverflowButton(this);
        menu_ = new page_menu_t(more_, "OverflowSettingsTabs");
        menu_->SetNumberOfVisibleItems(kMenuVisibleItems);
        more_->SetMenu(menu_);
        SetKBNavigationEnabled(true);
    }

    void AddPage(vgui2::Panel* page, const char* title, const char* image = nullptr, bool context_menu = false) override
    {
        navigation_.AddPage(page);
        BaseClass::AddPage(page, title, image, context_menu);
        // VGUI exposes only the active tab. Bind each newly created tab to its
        // page once, independently of translated or duplicate captions.
        vgui2::Button* tab = nullptr;
        for (int index = 0; index < GetChildCount(); ++index)
        {
            vgui2::Panel* child = GetChild(index);
            if (!strcmp(child->GetName(), "tab") &&
                std::none_of(tabs_.begin(), tabs_.end(), [child](const auto& entry) { return entry.second.button == child; }))
            {
                tab = static_cast<vgui2::Button*>(child);
                break;
            }
        }
        assert(tab);
        tabs_.emplace(page, PageTab{tab, title});
        tab->GetTooltip()->SetText(title);
        tab->GetTooltip()->SetEnabled(false);
        menu_dirty_ = true;
        InvalidateLayout();
    }

    void RemovePage(vgui2::Panel* page) override
    {
        more_->HideMenu();
        navigation_.RemovePage(page);
        tabs_.erase(page);
        menu_dirty_ = true;
        BaseClass::RemovePage(page);
        page->RemoveActionSignalTarget(this);
        if (!GetActiveTab())
        {
            const int first = navigation_.FindEnabled(0);
            if (first >= 0)
            {
                SetActivePage(GetPage(first));
            }
        }
    }

    void SetPageEnabled(vgui2::Panel* page, bool enabled)
    {
        navigation_.SetPageEnabled(page, enabled);
        PageTab& tab = tabs_.at(page);
        tab.button->SetEnabled(enabled);
        tab.button->SetKeyBoardInputEnabled(enabled);
        if (tab.menu_id >= 0)
        {
            menu_->SetItemEnabled(tab.menu_id, enabled);
        }
        if (!enabled && GetActivePage() == page)
        {
            const int first = navigation_.FindEnabled(0);
            if (first >= 0)
            {
                SetActivePage(GetPage(first));
            }
        }
        InvalidateLayout();
    }

    void HideOverflowMenu()
    {
        more_->HideMenu();
    }

    bool RequestFocusNext(vgui2::VPANEL panel) override
    {
        if (more_->IsVisible() && GetActiveTab() && GetActiveTab()->HasFocus())
        {
            more_->RequestFocus();
            return true;
        }
        if (more_->HasFocus())
        {
            BaseClass::RequestFocus(1);
        }
        return BaseClass::RequestFocusNext(panel);
    }

    bool RequestFocusPrev(vgui2::VPANEL panel) override
    {
        if (more_->HasFocus())
        {
            BaseClass::RequestFocus(1);
            return true;
        }
        if (more_->IsVisible() && GetActivePage() && GetActiveTab() && !GetActiveTab()->HasFocus())
        {
            more_->RequestFocus();
            return true;
        }
        return BaseClass::RequestFocusPrev(panel);
    }

protected:
    void ApplySchemeSettings(vgui2::IScheme* scheme) override
    {
        BaseClass::ApplySchemeSettings(scheme);
        for (auto& entry : tabs_)
        {
            entry.second.button->SetText(entry.second.title.c_str());
        }
        more_->SetText(kMoreTitle);
        menu_dirty_ = true;
        InvalidateLayout();
    }

    void OnThink() override
    {
        BaseClass::OnThink();
        for (const auto& entry : tabs_)
        {
            const PageTab& tab = entry.second;
            if (tab.caption != tab.button->GetTextImage()->GetUText() || tab.font != tab.button->GetFont())
            {
                menu_dirty_ = true;
                InvalidateLayout();
                return;
            }
        }
        if (more_caption_ != more_->GetTextImage()->GetUText())
        {
            menu_dirty_ = true;
            InvalidateLayout();
        }
    }

    void PerformLayout() override
    {
        // Child schemes must be ready before the stock sheet measures tab text.
        for (int index = 0; index < GetNumPages(); ++index)
        {
            tabs_.at(GetPage(index)).button->MakeReadyForUse();
        }
        more_->MakeReadyForUse();
        more_caption_ = more_->GetTextImage()->GetUText();
        BaseClass::PerformLayout();

        if (GetNumPages() == 0)
        {
            more_->HideMenu();
            more_->SetVisible(false);
            overflow_.clear();
            menu_->DeleteAllItems();
            return;
        }

        vgui2::Button* first_tab = tabs_.at(GetPage(0)).button;
        const int start = first_tab->GetXPos();
        const int spacing = GetNumPages() > 1 ? std::max(0, tabs_.at(GetPage(1)).button->GetXPos() - start - first_tab->GetWide()) : 1;
        std::vector<int> widths;
        for (int index = 0; index < GetNumPages(); ++index)
        {
            PageTab& tab = tabs_.at(GetPage(index));
            tab.caption = tab.button->GetTextImage()->GetUText();
            tab.font = tab.button->GetFont();
            widths.push_back(tab.button->GetWide());
        }
        more_->SetFont(first_tab->GetFont());
        const PropertyPageTabLayout layout =
            navigation_.CalculateLayout(widths, GetActivePage(), std::max(0, GetWide() - start), more_->CalculateWidth(), spacing);

        std::vector<vgui2::Panel*> overflow;
        int x = start;
        for (int index = 0; index < GetNumPages(); ++index)
        {
            vgui2::Panel* page = GetPage(index);
            vgui2::Button* tab = tabs_.at(page).button;
            const int width = layout.tab_widths[index];
            tab->SetVisible(width > 0);
            tab->SetMouseInputEnabled(width > 0);
            tab->SetKeyBoardInputEnabled(width > 0 && navigation_.is_page_enabled(page));
            if (width > 0)
            {
                tab->SetPos(x, tab->GetYPos());
                tab->SetWide(width);
                int inset_x, inset_y;
                tab->GetTextInset(&inset_x, &inset_y);
                tab->GetTextImage()->SetDrawWidth(std::max(0, width - 2 * inset_x));
                tab->GetTooltip()->SetEnabled(width < widths[index]);
                x += width + spacing;
            }
            else
            {
                overflow.push_back(page);
            }
        }

        if (overflow != overflow_ || more_->GetWide() != layout.more_width)
        {
            menu_dirty_ = true;
            overflow_ = std::move(overflow);
        }
        if (!layout.more_width && more_->HasFocus())
        {
            BaseClass::RequestFocus(1);
        }
        more_->SetVisible(layout.more_width > 0);
        more_->SetKeyBoardInputEnabled(layout.more_width > 0);
        more_->SetBounds(
            GetWide() - layout.more_width,
            kInactiveTabY,
            layout.more_width,
            std::max(0, first_tab->GetYPos() + first_tab->GetTall() - kInactiveTabY)
        );
        more_->MoveToFront();
        if (menu_dirty_)
        {
            RebuildOverflowMenu();
        }
    }

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
    struct PageTab
    {
        vgui2::Button* button;
        std::string title;
        int menu_id = -1;
        std::wstring caption;
        vgui2::HFont font{};
    };

    MESSAGE_FUNC_INT_INT(OnScreenSizeChanged, "OnScreenSizeChanged", oldwide, oldtall)
    {
        BaseClass::OnScreenSizeChanged(oldwide, oldtall);
        menu_dirty_ = true;
        InvalidateLayout();
    }

    void RebuildOverflowMenu()
    {
        more_->HideMenu();
        menu_->DeleteAllItems();
        for (auto& entry : tabs_)
        {
            entry.second.menu_id = -1;
        }
        for (vgui2::Panel* page : overflow_)
        {
            PageTab& tab = tabs_.at(page);
            KeyValues* command = new KeyValues("OverflowPagePicked");
            command->SetPtr("page", page);
            tab.menu_id = menu_->AddMenuItem(tab.title.c_str(), command, this);
            menu_->SetItemEnabled(tab.menu_id, navigation_.is_page_enabled(page));
            menu_->GetMenuItem(tab.menu_id)->GetTooltip()->SetText(tab.title.c_str());
            menu_->GetMenuItem(tab.menu_id)->MakeReadyForUse();
        }
        menu_->MakeReadyForUse();
        menu_->SetFixedWidth(0);
        menu_->SetMinimumWidth(more_->GetWide());
        menu_->ForceCalculateWidth();
        int work_x, work_y, work_wide, work_tall;
        vgui2::surface()->GetWorkspaceBounds(work_x, work_y, work_wide, work_tall);
        menu_->SetFixedWidth(std::min(menu_->GetWide(), work_wide));
        menu_dirty_ = false;
    }

    MESSAGE_FUNC_PTR(OnOverflowPagePicked, "OverflowPagePicked", page)
    {
        vgui2::Panel* selected = static_cast<vgui2::Panel*>(page);
        if (navigation_.is_page_enabled(selected))
        {
            SetActivePage(selected);
            InvalidateLayout(true);
            BaseClass::RequestFocus(1);
        }
    }

    PropertyPageNavigation navigation_;
    std::map<vgui2::Panel*, PageTab> tabs_;
    std::vector<vgui2::Panel*> overflow_;
    std::wstring more_caption_;
    CPageOverflowButton* more_;
    page_menu_t* menu_;
    bool menu_dirty_ = true;
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
    sheet_->AddPage(page, title);
}

void COverflowPropertyDialog::SetPageEnabled(vgui2::Panel* page, bool enabled)
{
    sheet_->SetPageEnabled(page, enabled);
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

void COverflowPropertyDialog::PerformLayout()
{
    BaseClass::PerformLayout();
    int x, y, wide, tall;
    BaseClass::GetPropertySheet()->GetBounds(x, y, wide, tall);
    sheet_->SetBounds(x, y, wide, tall);
    sheet_->InvalidateLayout();
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

void COverflowPropertyDialog::OnClose()
{
    sheet_->HideOverflowMenu();
    BaseClass::OnClose();
}
