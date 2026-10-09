#include <array>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GameUi/Controls/PropertyPagePicker.h"

namespace
{
    struct MenuItemProbe
    {
        bool enabled = true;
        bool IsEnabled() const
        {
            return enabled;
        }
    };

    class MenuProbe
    {
    public:
        virtual ~MenuProbe() = default;
        std::array<MenuItemProbe, 3> items;
        int highlighted = -1;
        int activated = 10;

        int GetItemCount() const
        {
            return 3;
        }
        int GetMenuID(int row) const
        {
            return row >= 0 && row < 3 ? 10 + row * 2 : -1;
        }
        int GetRowByItemId(int id) const
        {
            return id >= 10 && id <= 14 && id % 2 == 0 ? (id - 10) / 2 : -1;
        }
        int GetCurrentlyHighlightedItem() const
        {
            return highlighted;
        }
        void SilentActivateItem(int id)
        {
            const int row = GetRowByItemId(id);
            if (row >= 0 && items[row].enabled)
            {
                activated = id;
            }
        }
        void SetCurrentlyHighlightedItem(int id)
        {
            highlighted = id;
            SilentActivateItem(id);
        }
        void ClearCurrentlyHighlightedItem()
        {
            highlighted = -1;
        }
        MenuItemProbe* GetMenuItem(int id)
        {
            return &items.at(GetRowByItemId(id));
        }
        virtual void OnKeyCodeTyped(vgui2::KeyCode code)
        {
            int row = GetRowByItemId(highlighted);
            switch (code)
            {
                case vgui2::KEY_HOME:
                    row = 0;
                    break;
                case vgui2::KEY_END:
                    row = 2;
                    break;
                case vgui2::KEY_UP:
                case vgui2::KEY_PAGEUP:
                    --row;
                    break;
                case vgui2::KEY_DOWN:
                case vgui2::KEY_PAGEDOWN:
                    ++row;
                    break;
                default:
                    return;
            }
            highlighted = GetMenuID(row < 0 ? 2 : row >= 3 ? 0 : row);
        }
    };

    class PickerProbe
    {
    public:
        virtual ~PickerProbe() = default;
        PropertyPagePickerMenu<MenuProbe> menu;
        std::vector<int> picked;
        std::array<std::wstring, 3> captions{L"Same title", L"Plugins", L"Same title"};
        std::wstring text = captions[0];
        bool open{}, alt{};

        void Key(vgui2::KeyCode code)
        {
            OnKeyCodeTyped(code);
        }
        void Character(wchar_t character)
        {
            OnKeyTyped(character);
        }
        void Click(int row)
        {
            const int id = GetItemIDFromRow(row);
            if (menu.GetMenuItem(id)->IsEnabled())
            {
                menu.SilentActivateItem(id);
                OnMenuItemSelected();
            }
        }
        void NotifyNativeSelection()
        {
            OnMenuItemSelected();
        }
        void SilentActivateItemByRow(int row)
        {
            menu.SilentActivateItem(GetItemIDFromRow(row));
            text = captions.at(row);
            open = false;
        }
        virtual void DoClick()
        {
            if (open)
            {
                open = false;
                return;
            }
            for (int row = 0; row < 3; ++row)
            {
                if (text == captions[row])
                {
                    menu.SetCurrentlyHighlightedItem(GetItemIDFromRow(row));
                    break;
                }
            }
            open = true;
        }
        void PostActionSignal(KeyValues* message)
        {
            message->deleteThis();
        }
        int GetActiveItem() const
        {
            return menu.activated;
        }
        int GetItemIDFromRow(int row) const
        {
            return menu.GetMenuID(row);
        }
        int GetRowByItemId(int id) const
        {
            return menu.GetRowByItemId(id);
        }
        bool IsDropdownVisible() const
        {
            return open;
        }
        PropertyPagePickerMenu<MenuProbe>* GetMenu()
        {
            return &menu;
        }

    protected:
        virtual void OnMenuItemSelected()
        {
            text = captions.at(GetRowByItemId(GetActiveItem()));
            open = false;
        }
        virtual void OnKeyCodeTyped(vgui2::KeyCode code)
        {
            if (alt && (code == vgui2::KEY_UP || code == vgui2::KEY_DOWN))
            {
                DoClick();
            }
            else if (code == vgui2::KEY_ENTER)
            {
                Click(GetRowByItemId(menu.highlighted));
            }
            else
            {
                menu.OnKeyCodeTyped(code);
            }
        }
        virtual void OnKeyTyped(wchar_t character)
        {
            if (character == L'p')
            {
                menu.SetCurrentlyHighlightedItem(GetItemIDFromRow(1));
                text = captions[1];
                open = false;
            }
        }
    };

    class PagePickerProbe : public PropertyPagePicker<PickerProbe>
    {
    public:
        PagePickerProbe()
        {
            SetSelection(0);
        }

    protected:
        void SendPagePicked(int row) override
        {
            picked.push_back(row);
        }
    };
} // namespace

TEST(PropertyPagePicker, ClosedKeyboardNavigationSkipsDisabledAndCommitsEqualCaptionByRow)
{
    PagePickerProbe picker;
    picker.menu.items[1].enabled = false;
    picker.Key(vgui2::KEY_DOWN);
    EXPECT_EQ(picker.picked, (std::vector<int>{2}));
    EXPECT_EQ(picker.get_selection(), 2);
    EXPECT_EQ(picker.text, L"Same title");
    picker.Key(vgui2::KEY_UP);
    EXPECT_EQ(picker.picked.back(), 0);
    picker.Click(2);
    EXPECT_EQ(picker.picked.back(), 2);
}

TEST(PropertyPagePicker, OpenMenuSkipsDisabledButDoesNotCommitUntilSelection)
{
    PagePickerProbe picker;
    picker.menu.items[1].enabled = false;
    picker.DoClick();
    picker.menu.OnKeyCodeTyped(vgui2::KEY_DOWN);
    EXPECT_EQ(picker.menu.GetRowByItemId(picker.menu.highlighted), 2);
    EXPECT_EQ(picker.get_selection(), 0);
    EXPECT_TRUE(picker.picked.empty());
    picker.Click(2);
    EXPECT_EQ(picker.picked, (std::vector<int>{2}));
}

TEST(PropertyPagePicker, HomeEndAndDisabledTypeaheadKeepAnEnabledSelection)
{
    PagePickerProbe picker;
    picker.menu.items[0].enabled = false;
    picker.Key(vgui2::KEY_HOME);
    EXPECT_EQ(picker.picked.back(), 1);
    picker.menu.items[2].enabled = false;
    picker.Key(vgui2::KEY_END);
    EXPECT_EQ(picker.picked.back(), 1);
    picker.menu.items[0].enabled = true;
    picker.menu.items[1].enabled = false;
    picker.SetSelection(0);
    const size_t count = picker.picked.size();
    picker.Character(L'p');
    EXPECT_EQ(picker.picked.size(), count);
    EXPECT_EQ(picker.text, L"Same title");
    EXPECT_EQ(picker.get_selection(), 0);
    EXPECT_EQ(picker.GetRowByItemId(picker.menu.highlighted), 0);
}

TEST(PropertyPagePicker, TabDoesNotCommitHoveredPage)
{
    PagePickerProbe picker;
    picker.menu.highlighted = picker.menu.GetMenuID(2);
    picker.Character(L'\t');
    EXPECT_TRUE(picker.picked.empty());
    EXPECT_EQ(picker.get_selection(), 0);
}

TEST(PropertyPagePicker, KeyboardNavigationStartsAtTheProgrammaticallySelectedPage)
{
    PagePickerProbe picker;
    picker.SetSelection(1);
    ASSERT_TRUE(picker.picked.empty());
    picker.Key(vgui2::KEY_DOWN);
    EXPECT_EQ(picker.picked, (std::vector<int>{2}));
    EXPECT_EQ(picker.get_selection(), 2);
}

TEST(PropertyPagePicker, OpeningDuplicateCaptionAndPressingEnterKeepsTheSelectedPage)
{
    PagePickerProbe picker;
    picker.SetSelection(2);
    picker.DoClick();
    EXPECT_EQ(picker.GetRowByItemId(picker.menu.highlighted), 2);
    picker.Key(vgui2::KEY_ENTER);
    EXPECT_EQ(picker.get_selection(), 2);
    EXPECT_EQ(picker.GetRowByItemId(picker.GetActiveItem()), 2);
    EXPECT_TRUE(picker.picked.empty());
}

TEST(PropertyPagePicker, ProgrammaticNotificationsDoNotReactivateThePage)
{
    PagePickerProbe picker;
    picker.SetSelection(2);
    picker.NotifyNativeSelection();
    picker.NotifyNativeSelection();
    EXPECT_TRUE(picker.picked.empty());
    EXPECT_EQ(picker.get_selection(), 2);
    picker.Click(1);
    picker.NotifyNativeSelection();
    EXPECT_EQ(picker.picked, (std::vector<int>{1}));
}

TEST(PropertyPagePicker, CancelledMenuPreviewDoesNotChangeWhereKeyboardNavigationStarts)
{
    PagePickerProbe picker;
    picker.DoClick();
    picker.menu.OnKeyCodeTyped(vgui2::KEY_END);
    ASSERT_EQ(picker.GetRowByItemId(picker.GetActiveItem()), 2);
    picker.open = false;
    picker.Key(vgui2::KEY_DOWN);
    EXPECT_EQ(picker.picked, (std::vector<int>{1}));
}

TEST(PropertyPagePicker, AltClosingTheMenuDiscardsItsPreview)
{
    PagePickerProbe picker;
    picker.DoClick();
    picker.menu.OnKeyCodeTyped(vgui2::KEY_END);
    picker.alt = true;
    picker.Key(vgui2::KEY_UP);
    EXPECT_FALSE(picker.IsDropdownVisible());
    EXPECT_EQ(picker.get_selection(), 0);
    EXPECT_EQ(picker.GetRowByItemId(picker.GetActiveItem()), 0);
    EXPECT_TRUE(picker.picked.empty());
}
