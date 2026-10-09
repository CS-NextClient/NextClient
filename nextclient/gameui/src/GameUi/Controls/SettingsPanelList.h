#pragma once

#include <vgui_controls/PanelListPanel.h>

class CSettingsPanelList : public vgui2::PanelListPanel
{
public:
    CSettingsPanelList(vgui2::Panel* parent, const char* name) :
        vgui2::PanelListPanel(parent, name)
    {
        SetTabPosition(1);
        FindChildByName("PanelListEmbedded")->SetTabPosition(1);
    }
};
