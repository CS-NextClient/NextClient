#pragma once
#include <vgui_controls/Label.h>
#include <vgui_controls/TextImage.h>
#include <algorithm>

class CWrappedLabel : public vgui2::Label
{
public:
    using vgui2::Label::Label;

    void PerformLayout() override
    {
        int insetX, insetY;
        GetTextInset(&insetX, &insetY);
        // VGUI measures wrapped height before updating the text image width.
        // Set the current width first so the initial layout is already correct.
        GetTextImage()->SetDrawWidth(std::max(1, GetWide() - insetX));
        vgui2::Label::PerformLayout();
    }
};
