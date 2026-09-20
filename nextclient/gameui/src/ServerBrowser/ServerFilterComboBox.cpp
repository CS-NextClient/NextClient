#include "ServerFilterComboBox.h"

#include <algorithm>
#include <string>
#include <utility>

#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>

namespace
{
    // pixels between an item's text and its count, and between the count and the item's right edge
    constexpr int kCountGap = 8;
    constexpr int kCountRightInset = 5;

    class CServerFilterMenuItem : public vgui2::MenuItem
    {
        DECLARE_CLASS_SIMPLE(CServerFilterMenuItem, vgui2::MenuItem);

    private:
        std::wstring count_text_{};

    public:
        CServerFilterMenuItem(vgui2::Menu* parent, const char* name, const char* text) :
            BaseClass(parent, name, text)
        {}

        // Whether the count text changed
        bool SetCount(int count)
        {
            std::wstring text = std::to_wstring(count);

            if (text == count_text_)
            {
                return false;
            }

            count_text_ = std::move(text);
            Repaint();

            return true;
        }

        void Paint() override
        {
            BaseClass::Paint();

            if (count_text_.empty())
            {
                return;
            }

            int wide;
            int tall;
            GetSize(wide, tall);

            int count_wide;
            int count_tall;
            vgui2::surface()->GetTextSize(GetFont(), count_text_.c_str(), count_wide, count_tall);

            vgui2::surface()->DrawSetTextFont(GetFont());
            vgui2::surface()->DrawSetTextColor(GetButtonFgColor());
            vgui2::surface()->DrawSetTextPos(wide - count_wide - kCountRightInset, (tall - count_tall) / 2);
            vgui2::surface()->DrawPrintText(count_text_.c_str(), static_cast<int>(count_text_.size()));
        }

        void GetContentSize(int& wide, int& tall) override
        {
            BaseClass::GetContentSize(wide, tall);

            if (count_text_.empty())
            {
                return;
            }

            int count_wide;
            int count_tall;
            vgui2::surface()->GetTextSize(GetFont(), count_text_.c_str(), count_wide, count_tall);

            wide += kCountGap + count_wide + kCountRightInset;
            tall = std::max(tall, count_tall);
        }
    };

    class CServerFilterMenu : public vgui2::Menu
    {
        DECLARE_CLASS_SIMPLE(CServerFilterMenu, vgui2::Menu);

    private:
        int lines_{};

    public:
        explicit CServerFilterMenu(vgui2::Panel* parent) :
            BaseClass(parent, nullptr)
        {}

        using BaseClass::AddMenuItem;

        // Makes every item a CServerFilterMenuItem: ComboBox::AddItem adds all its items through this overload
        int AddMenuItem(const char* name, const char* text, KeyValues* message, vgui2::Panel* target, const KeyValues* user_data) override
        {
            return AddMenuItemKeyValuesCommand(new CServerFilterMenuItem(this, name, text), message, target, user_data);
        }

        void SetNumberOfVisibleItems(int lines) override
        {
            BaseClass::SetNumberOfVisibleItems(lines);

            lines_ = lines;
        }

        // Takes the given width or the one the widest item needs, whichever is larger: ComboBox pins its drop-down to
        // the width of the box through this setter
        void SetFixedWidth(int width) override
        {
            int widest = 0;

            for (int row = 0; row < GetItemCount(); row++)
            {
                int item_wide;
                int item_tall;
                GetMenuItem(GetMenuID(row))->GetContentSize(item_wide, item_tall);

                widest = std::max(widest, item_wide);
            }

            // the padding Menu gives its widest item without a fixed width; a fixed width takes in the scroll bar
            int fit_width = widest + vgui2::Label::Content;

            if (NeedsScrollBar())
            {
                fit_width += FindChildByName("MenuScrollBar")->GetWide();
            }

            BaseClass::SetFixedWidth(std::max(width, fit_width));
        }

    private:
        // The scroll bar test of Menu::PerformLayout
        bool NeedsScrollBar()
        {
            int work_wide;
            int work_tall;
            ComputeWorkspaceSize(work_wide, work_tall);

            return (lines_ > 0 && CountVisibleItems() > lines_) || ComputeFullMenuHeightWithInsets() >= work_tall;
        }
    };
} // namespace

CServerFilterComboBox::CServerFilterComboBox(vgui2::Panel* parent, const char* name, int lines, bool editable) :
    BaseClass(parent, name, lines, editable)
{
    // the replacement drop-down needs the setup ComboBox gave the one it created
    SetMenu(new CServerFilterMenu(this));
    GetMenu()->AddActionSignalTarget(this);
    GetMenu()->SetTypeAheadMode(vgui2::Menu::COMPAT_MODE);
    SetNumberOfEditLines(lines);
}

void CServerFilterComboBox::SetItemCount(int item_id, int count)
{
    CServerFilterMenuItem* item = dynamic_cast<CServerFilterMenuItem*>(GetMenu()->GetMenuItem(item_id));

    // the box sets the width of its drop-down in its layout
    if (item->SetCount(count) && IsDropdownVisible())
    {
        InvalidateLayout();
    }
}

void CServerFilterComboBox::OnKeyCodeTyped(vgui2::KeyCode code)
{
    int highlighted_item = GetMenu()->GetCurrentlyHighlightedItem();

    BaseClass::OnKeyCodeTyped(code);

    // ComboBox shows the item the arrow, Home, End and page keys move to without making it the active item
    if (GetMenu()->GetCurrentlyHighlightedItem() != highlighted_item)
    {
        GetMenu()->SilentActivateItem(GetMenu()->GetCurrentlyHighlightedItem());
    }
}
