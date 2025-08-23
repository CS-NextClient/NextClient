#pragma once

#include "vgui_controls/PropertyDialog.h"
#include "vgui_controls/KeyRepeat.h"
#include "utldict.h"

class COptionsDialog : public vgui2::PropertyDialog
{
    DECLARE_CLASS_SIMPLE(COptionsDialog, vgui2::PropertyDialog);

    CUtlDict<vgui2::PropertyPage*> m_tabNames;

public:
    COptionsDialog(vgui2::Panel *parent);
    ~COptionsDialog(void);

    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OpenTab(const char* tabName);

public:
    void Activate(void);

public:
    void OnClose(void);

public:
    MESSAGE_FUNC(OnGameUIHidden, "GameUIHidden");

private:
    class COptionsSubMultiplayer *m_pOptionsSubMultiplayer;
    class COptionsSubKeyboard *m_pOptionsSubKeyboard;
    class COptionsSubMouse *m_pOptionsSubMouse;
    class COptionsSubAudio *m_pOptionsSubAudio;
    class COptionsSubVideo *m_pOptionsSubVideo;
    class COptionsSubVoice *m_pOptionsSubVoice;
    class OptionsSubMiscellaneous *m_pOptionsSubMiscellaneous;
};