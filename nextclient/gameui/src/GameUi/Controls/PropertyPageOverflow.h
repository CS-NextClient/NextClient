#pragma once

#include <vgui/KeyCode.h>

template <class Menu>
class PropertyPageOverflowMenu : public Menu
{
public:
    using Menu::Menu;

    void HighlightFirstEnabled()
    {
        HighlightEnabled(0, 1);
    }

    void OnKeyCodeTyped(vgui2::KeyCode code) override
    {
        if (code == vgui2::KEY_ESCAPE || code == vgui2::KEY_XBUTTON_B)
        {
            this->SetVisible(false);
            this->GetParent()->RequestFocus();
            return;
        }
        if (code == vgui2::KEY_ENTER || code == vgui2::KEY_XBUTTON_A)
        {
            const int id = this->GetCurrentlyHighlightedItem();
            if (this->GetRowByItemId(id) < 0 || !this->GetMenuItem(id)->IsEnabled())
            {
                return;
            }
        }
        Menu::OnKeyCodeTyped(code);
        int direction = 0;
        switch (code)
        {
            case vgui2::KEY_UP:
            case vgui2::KEY_XBUTTON_UP:
            case vgui2::KEY_XSTICK1_UP:
            case vgui2::KEY_PAGEUP:
            case vgui2::KEY_END:
                direction = -1;
                break;
            case vgui2::KEY_DOWN:
            case vgui2::KEY_XBUTTON_DOWN:
            case vgui2::KEY_XSTICK1_DOWN:
            case vgui2::KEY_PAGEDOWN:
            case vgui2::KEY_HOME:
                direction = 1;
                break;
            default:
                return;
        }
        HighlightEnabled(this->GetRowByItemId(this->GetCurrentlyHighlightedItem()), direction);
    }

private:
    void HighlightEnabled(int start, int direction)
    {
        const int count = this->GetItemCount();
        for (int offset = 0; offset < count; ++offset)
        {
            const int row = ((start + offset * direction) % count + count) % count;
            const int id = this->GetMenuID(row);
            if (this->GetMenuItem(id)->IsEnabled())
            {
                this->SetCurrentlyHighlightedItem(id);
                return;
            }
        }
        this->ClearCurrentlyHighlightedItem();
    }
};

template <class Button>
class PropertyPageOverflowButton : public Button
{
public:
    using Button::Button;

    void OnKeyCodePressed(vgui2::KeyCode code) override
    {
        if (code == vgui2::KEY_ENTER || code == vgui2::KEY_SPACE)
        {
            if (this->IsEnabled())
            {
                this->DoClick();
            }
            return;
        }
        Button::OnKeyCodePressed(code);
    }

    void OnKeyCodeTyped(vgui2::KeyCode code) override
    {
        if (code != vgui2::KEY_ENTER && code != vgui2::KEY_SPACE)
        {
            Button::OnKeyCodeTyped(code);
        }
    }

    void OnKeyCodeReleased(vgui2::KeyCode code) override
    {
        if (code != vgui2::KEY_ENTER && code != vgui2::KEY_SPACE)
        {
            Button::OnKeyCodeReleased(code);
        }
    }

    void HideMenu() override
    {
        // Input focus changes on the next frame, after posted MenuClose messages.
        const bool had_focus = this->GetMenu()->IsVisible() && this->GetMenu()->HasFocus();
        Button::HideMenu();
        if (had_focus && this->IsVisible() && this->IsEnabled())
        {
            for (auto* owner = this->GetParent(); owner; owner = owner->GetParent())
            {
                if (!owner->IsVisible())
                {
                    return;
                }
            }
            this->RequestFocus();
        }
    }
};
