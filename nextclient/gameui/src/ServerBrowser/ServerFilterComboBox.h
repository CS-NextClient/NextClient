#pragma once

#include <vgui_controls/ComboBox.h>

// Combo box of a server filter: its drop-down shows a server count to the right of every item and grows wider than
// the box when an item needs the room
class CServerFilterComboBox : public vgui2::ComboBox
{
    DECLARE_CLASS_SIMPLE(CServerFilterComboBox, vgui2::ComboBox);

public:
    CServerFilterComboBox(vgui2::Panel* parent, const char* name, int lines, bool editable);

    void SetItemCount(int item_id, int count);

    // Panel
    void OnKeyCodeTyped(vgui2::KeyCode code) override;
};
