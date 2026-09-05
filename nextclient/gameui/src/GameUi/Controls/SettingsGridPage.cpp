#include "SettingsGridPage.h"

#include <algorithm>

#include <vgui_controls/Label.h>

namespace
{
    constexpr int kMargin = 10;
    constexpr int kValueWide = 48;
    constexpr int kValueGap = 6;
    constexpr int kColumnGap = 16;
    constexpr int kRowGap = 10;
    constexpr int kMaxColumns = 3;
    constexpr int kMinCellWide = 170;

    int MaxColumns(int usable_wide)
    {
        return std::clamp(usable_wide / kMinCellWide, 1, kMaxColumns);
    }

    int CellWide(int usable_wide, int columns)
    {
        return (usable_wide - kColumnGap * (columns - 1)) / columns;
    }
}

CSettingsGridPage::CSettingsGridPage(vgui2::Panel* parent, const char* panel_name) :
    BaseClass(parent, panel_name)
{
}

void CSettingsGridPage::AddCell(const char* caption, vgui2::Panel* control, vgui2::Panel* value_entry, int control_tall)
{
    vgui2::Label* label = new vgui2::Label(this, "", caption);
    label->SetContentAlignment(vgui2::Label::a_west);

    cells_.push_back(Cell{label, control, value_entry, control_tall, false});
}

void CSettingsGridPage::AddWideCell(vgui2::Label* control, int control_tall)
{
    cells_.push_back(Cell{nullptr, control, nullptr, control_tall, true});
}

// The captions share one font, so the first one answers for the height of all of them.
int CSettingsGridPage::MeasureCaptionTall()
{
    for (const Cell& cell : cells_)
    {
        if (cell.caption == nullptr)
            continue;

        int caption_wide, caption_tall;
        cell.caption->GetContentSize(caption_wide, caption_tall);

        return caption_tall;
    }

    return 0;
}

int CSettingsGridPage::MeasureGridRowTall(int caption_tall)
{
    int grid_row_tall = 0;

    for (const Cell& cell : cells_)
    {
        if (cell.wide || !cell.control->IsVisible())
            continue;

        grid_row_tall = std::max(grid_row_tall, caption_tall + cell.control_tall);
    }

    return grid_row_tall;
}

// A wide cell holds the label AddWideCell took, so the control knows the width its caption needs.
int CSettingsGridPage::MeasureCellWide(const Cell& cell)
{
    int content_wide, content_tall;
    static_cast<vgui2::Label*>(cell.control)->GetContentSize(content_wide, content_tall);

    return content_wide;
}

int CSettingsGridPage::PackCells(int usable_wide, int columns, int caption_tall, int grid_row_tall, std::vector<Slot>& slots)
{
    int cell_wide = CellWide(usable_wide, columns);
    int column = 0;
    int y = 0;

    slots.clear();

    for (const Cell& cell : cells_)
    {
        if (!cell.control->IsVisible())
            continue;

        int x = column * (cell_wide + kColumnGap);

        if (!cell.wide)
        {
            slots.push_back(Slot{&cell, x, y, cell_wide, false});
            column++;

            if (column == columns)
            {
                column = 0;
                y += grid_row_tall + kRowGap;
            }

            continue;
        }

        int remainder_wide = usable_wide - x;

        if (column != 0 && MeasureCellWide(cell) <= remainder_wide)
        {
            slots.push_back(Slot{&cell, x, y, remainder_wide, true});
            y += std::max(grid_row_tall, caption_tall + cell.control_tall) + kRowGap;
        }
        else
        {
            if (column != 0)
                y += grid_row_tall + kRowGap;

            slots.push_back(Slot{&cell, 0, y, usable_wide, false});
            y += cell.control_tall + kRowGap;
        }

        column = 0;
    }

    if (column != 0)
        y += grid_row_tall + kRowGap;

    return std::max(0, y - kRowGap);
}

int CSettingsGridPage::MeasurePreferredTall(int wide)
{
    int usable_wide = wide - 2 * kMargin;

    if (usable_wide <= 0)
        return 0;

    int caption_tall = MeasureCaptionTall();
    std::vector<Slot> slots;

    return PackCells(usable_wide, MaxColumns(usable_wide), caption_tall, MeasureGridRowTall(caption_tall), slots) + 2 * kMargin;
}

void CSettingsGridPage::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide, tall;
    GetSize(wide, tall);

    int usable_wide = wide - 2 * kMargin;
    int usable_tall = tall - 2 * kMargin;

    if (usable_wide <= 0 || cells_.empty())
        return;

    for (const Cell& cell : cells_)
    {
        bool visible = cell.control->IsVisible();

        if (cell.caption)
            cell.caption->SetVisible(visible);

        if (cell.value)
            cell.value->SetVisible(visible);
    }

    int caption_tall = MeasureCaptionTall();
    int grid_row_tall = MeasureGridRowTall(caption_tall);
    int max_columns = MaxColumns(usable_wide);
    std::vector<Slot> slots;

    // the cells are as wide as the page allows, so the fewest columns they still fit in win
    for (int columns = 1; columns <= max_columns; columns++)
    {
        if (PackCells(usable_wide, columns, caption_tall, grid_row_tall, slots) <= usable_tall || columns == max_columns)
            break;
    }

    for (const Slot& slot : slots)
    {
        const Cell& cell = *slot.cell;
        int x = kMargin + slot.x;
        int y = kMargin + slot.y;

        if (cell.wide)
        {
            int control_y = slot.on_control_line ? y + caption_tall : y;
            cell.control->SetBounds(x, control_y, slot.wide, cell.control_tall);
            continue;
        }

        int control_wide = slot.wide;
        int control_y = y + caption_tall;

        cell.caption->SetBounds(x, y, slot.wide, caption_tall);

        if (cell.value)
        {
            control_wide -= kValueWide + kValueGap;
            cell.value->SetBounds(x + control_wide + kValueGap, control_y + 4, kValueWide, 20);
        }

        cell.control->SetBounds(x, control_y, control_wide, cell.control_tall);
    }
}
