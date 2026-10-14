#include "HudTextZone.h"

#include <algorithm>

#include <ncl_math/vec3.h>

namespace
{
    bool g_ZoneActive;
    ncl_math::Vector2 g_ZoneOrigin;
    float g_ZoneSize;
    bool g_Passthrough;
    int g_ScreenW = 1024;
    int g_ScreenH = 768;

    constexpr int kGap = 12;
} // namespace


void HudTextZone_SetZone(float x, float y, float size)
{
    g_ZoneActive = true;
    g_ZoneOrigin = {x, y};
    g_ZoneSize = size;
}

void HudTextZone_ClearZone()
{
    g_ZoneActive = false;
}

void HudTextZone_SetPassthrough(bool on)
{
    g_Passthrough = on;
}

void HudTextZone_BeginFrame(int screen_width, int screen_height)
{
    if (screen_width > 0)
    {
        g_ScreenW = screen_width;
    }

    if (screen_height > 0)
    {
        g_ScreenH = screen_height;
    }
}

bool HudTextZone_IsActive()
{
    return g_ZoneActive && !g_Passthrough;
}

void HudTextZone_ComputeBlockShifts(std::span<HudTextLine> lines)
{
    int row_gap = std::max(32, g_ScreenH / 24);

    for (HudTextLine& line : lines)
    {
        int top = line.y;
        int bottom = line.y;
        int width = line.width;
        bool expanded = true;

        // Matching anchors and nearby rows identify a block when the engine only supplies glyphs.
        while (expanded)
        {
            expanded = false;

            for (const HudTextLine& other : lines)
            {
                if (other.left != line.left || other.y < top - row_gap || other.y > bottom + row_gap)
                {
                    continue;
                }

                int next_top = std::min(top, other.y);
                int next_bottom = std::max(bottom, other.y);
                expanded = expanded || next_top != top || next_bottom != bottom;
                top = next_top;
                bottom = next_bottom;
                width = std::max(width, other.width);
            }
        }

        line.dx = 0;
        line.dy = 0;

        for (const HudTextLine& other : lines)
        {
            if (other.left == line.left && other.y >= top && other.y <= bottom && HudTextZone_IsRowInZone(other.y))
            {
                HudTextZone_ComputeLineShift(line.left, other.y, width, line.dx, line.dy);

                if (line.dy)
                {
                    line.dy = (int)(g_ZoneOrigin.y + g_ZoneSize) + kGap - top;
                }

                if (line.dx || line.dy)
                {
                    break;
                }
            }
        }
    }
}

bool HudTextZone_IsRowInZone(int y)
{
    return g_ZoneActive && !g_Passthrough && (float)y >= g_ZoneOrigin.y - kGap && (float)y <= g_ZoneOrigin.y + g_ZoneSize + kGap;
}

void HudTextZone_ComputeLineShift(int left, int y, int width, int& dx, int& dy)
{
    dx = 0;
    dy = 0;

    if (!HudTextZone_IsRowInZone(y))
    {
        return;
    }

    float zleft = g_ZoneOrigin.x, zright = g_ZoneOrigin.x + g_ZoneSize;

    if (left + width <= (int)zleft - kGap || (float)left >= zright + kGap)
    {
        return;
    }

    if ((int)zright + kGap + width <= g_ScreenW)
    {
        dx = (int)zright + kGap - left;
    }
    else
    {
        dy = (int)(g_ZoneOrigin.y + g_ZoneSize) + kGap - y;
    }
}

bool HudTextZone_ShiftString(int& x, int& y, int width)
{
    int dx, dy;
    HudTextZone_ComputeLineShift(x, y, width, dx, dy);

    if (!dx && !dy)
    {
        return false;
    }

    x += dx;
    y += dy;
    return true;
}
