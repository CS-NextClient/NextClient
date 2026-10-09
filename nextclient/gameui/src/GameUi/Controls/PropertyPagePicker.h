#pragma once

#include <KeyValues.h>
#include <vgui/KeyCode.h>

// Native menu navigation can highlight disabled items. The picker must also
// commit keyboard choices by item identity, even when captions are equal.
template <class Menu>
class PropertyPagePickerMenu : public Menu
{
public:
    using Menu::Menu;
    void OnKeyCodeTyped(vgui2::KeyCode code) override
    {
        Menu::OnKeyCodeTyped(code);
        int direction = 0;
        switch (code)
        {
            case vgui2::KEY_UP:
            case vgui2::KEY_PAGEUP:
            case vgui2::KEY_END:
                direction = -1;
                break;
            case vgui2::KEY_DOWN:
            case vgui2::KEY_PAGEDOWN:
            case vgui2::KEY_HOME:
                direction = 1;
                break;
            default:
                return;
        }
        const int count = this->GetItemCount();
        const int start = this->GetRowByItemId(this->GetCurrentlyHighlightedItem());
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

template <class ComboBox>
class PropertyPagePicker : public ComboBox
{
public:
    using ComboBox::ComboBox;

    void SetSelection(int row)
    {
        selection_ = row;
        if (row >= 0)
        {
            this->SilentActivateItemByRow(row);
        }
        SynchronizeHighlight();
    }

    int get_selection() const
    {
        return selection_;
    }

    void DoClick() override
    {
        ComboBox::DoClick();
        if (this->IsDropdownVisible())
        {
            // Native dropdown opening matches captions; restore the committed row.
            SynchronizeHighlight();
        }
        else
        {
            SetSelection(selection_);
        }
    }

protected:
    virtual void SendPagePicked(int row)
    {
        this->PostActionSignal(new KeyValues("PagePicked", "row", row));
    }

    void OnMenuItemSelected() override
    {
        ComboBox::OnMenuItemSelected();
        SelectPage(this->GetRowByItemId(this->GetActiveItem()));
    }

    void OnKeyCodeTyped(vgui2::KeyCode code) override
    {
        if (!this->IsDropdownVisible())
        {
            SynchronizeHighlight();
        }
        ComboBox::OnKeyCodeTyped(code);
        switch (code)
        {
            case vgui2::KEY_UP:
            case vgui2::KEY_DOWN:
            case vgui2::KEY_PAGEUP:
            case vgui2::KEY_PAGEDOWN:
            case vgui2::KEY_HOME:
            case vgui2::KEY_END:
                if (!this->IsDropdownVisible())
                {
                    SelectHighlightedPage();
                }
                break;
            default:
                break;
        }
    }

    void OnKeyTyped(wchar_t character) override
    {
        if (character != L'\t' && !this->IsDropdownVisible())
        {
            SynchronizeHighlight();
        }
        ComboBox::OnKeyTyped(character);
        if (character != L'\t' && !this->IsDropdownVisible())
        {
            SelectHighlightedPage();
        }
    }

private:
    void SynchronizeHighlight()
    {
        if (selection_ >= 0)
        {
            this->GetMenu()->SetCurrentlyHighlightedItem(this->GetItemIDFromRow(selection_));
        }
        else
        {
            this->GetMenu()->ClearCurrentlyHighlightedItem();
        }
    }

    void SelectPage(int row)
    {
        const int previous = selection_;
        if (row >= 0 && this->GetMenu()->GetMenuItem(this->GetItemIDFromRow(row))->IsEnabled())
        {
            SetSelection(row);
            if (row != previous)
            {
                SendPagePicked(row);
            }
        }
        else
        {
            SetSelection(previous);
        }
    }

    void SelectHighlightedPage()
    {
        SelectPage(this->GetRowByItemId(this->GetMenu()->GetCurrentlyHighlightedItem()));
    }

    int selection_ = -1;
};
