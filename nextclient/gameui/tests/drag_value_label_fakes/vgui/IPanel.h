#pragma once
#include <vgui_controls/Panel.h>
namespace vgui2
{
    class IPanel
    {
    public:
        void GetClipRect(VPANEL, int& x0, int& y0, int& x1, int& y1)
        {
            x0 = y0 = 0;
            x1 = y1 = 128;
        }
    };
    IPanel* ipanel();
} // namespace vgui2
