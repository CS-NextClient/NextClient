#pragma once

#include <vector>

#include <vgui_controls/PropertyPage.h>

namespace vgui2
{
    class Label;
}

// A settings page that arranges its controls in a grid computed from its own size:
// a cell is a caption with a control under it and an optional value entry beside it.
// The column count is chosen so every cell fits vertically, which makes the cells as
// wide as the page allows. Cells whose control is hidden are skipped, so hiding a
// control reflows the grid instead of leaving a hole.
class CSettingsGridPage : public vgui2::PropertyPage
{
    DECLARE_CLASS_SIMPLE(CSettingsGridPage, vgui2::PropertyPage);

    struct Cell
    {
        vgui2::Label* caption;
        vgui2::Panel* control;
        vgui2::Panel* value;
        int control_tall;
        bool wide;
    };

    // Where a visible cell goes, relative to the content's top left corner: the top of its
    // row and the span it may fill. A wide cell that joined a row of captioned cells sits on
    // their control line.
    struct Slot
    {
        const Cell* cell;
        int x;
        int y;
        int wide;
        bool on_control_line;
    };

private:
    std::vector<Cell> cells_;

public:
    CSettingsGridPage(vgui2::Panel* parent, const char* panel_name);

    // value_entry may be null; control_tall is the height the control needs
    void AddCell(const char* caption, vgui2::Panel* control, vgui2::Panel* value_entry, int control_tall);

    // a control that carries its own caption, like a check button: it takes the columns left
    // in the row above it when its caption fits them, and a row of its own otherwise
    void AddWideCell(vgui2::Label* control, int control_tall);

    // the page height the visible cells need at the given page width
    int MeasurePreferredTall(int wide);

    void PerformLayout() override;

private:
    int MeasureCaptionTall();
    int MeasureGridRowTall(int caption_tall);
    int MeasureCellWide(const Cell& cell);

    // Packs the visible cells into the given number of columns; returns the content height.
    int PackCells(int usable_wide, int columns, int caption_tall, int grid_row_tall, std::vector<Slot>& slots);
};
