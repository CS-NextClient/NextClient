#include "HudTextZone.h"

#include <ncl_math/vec3.h>

namespace
{
    bool g_ZoneActive;
    ncl_math::Vector2 g_ZoneOrigin;
    float g_ZoneSize;
    bool g_Passthrough;
    int g_ScreenW = 1024;
    int g_ScreenH = 768;

    constexpr int kGap = 4;
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

bool HudTextZone_IsRowInZone(int y)
{
    return g_ZoneActive && !g_Passthrough && (float)y >= g_ZoneOrigin.y && (float)y <= g_ZoneOrigin.y + g_ZoneSize;
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

    if (left + width <= (int)zleft || (float)left >= zright)
    {
        return;
    }

    if ((int)zright + kGap + width <= g_ScreenW)
    {
        dx = (int)zright + kGap - left;
    }
    else
    {
        // A fixed additive drop keeps a stack of lines at their original row spacing
        // instead of collapsing them onto the zone's bottom edge.
        dy = (int)g_ZoneSize + kGap;
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

