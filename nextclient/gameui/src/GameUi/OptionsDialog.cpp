#include "OptionsDialog.h"

#include <GameUi.h>

#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/QueryBox.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurfaceNext.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "OptionsSubMultiplayer.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubAudio.h"
#include "OptionsSubVoice.h"
#include "OptionsSubVideo.h"
#include "OptionsSubMiscellaneous.h"

#include "ModInfo.h"

#include "KeyValues.h"

COptionsDialog::COptionsDialog(vgui2::Panel *parent) : PropertyDialog(parent, "OptionsDialog")
{
    SetBounds(0, 0, 531, 406);
    SetSizeable(false);
    SetTitle("#GameUI_Options", true);

    m_pOptionsSubMultiplayer = NULL;
    m_pOptionsSubKeyboard = NULL;
    m_pOptionsSubMouse = NULL;
    m_pOptionsSubAudio = NULL;
    m_pOptionsSubVideo = NULL;
    m_pOptionsSubVoice = NULL;
    m_pOptionsSubMiscellaneous = NULL;

    if ((ModInfo().IsMultiplayerOnly() && !ModInfo().IsSinglePlayerOnly()) || (!ModInfo().IsMultiplayerOnly() && !ModInfo().IsSinglePlayerOnly()))
        m_pOptionsSubMultiplayer = new COptionsSubMultiplayer(this);

    m_pOptionsSubKeyboard = new COptionsSubKeyboard(this);
    m_pOptionsSubMouse = new COptionsSubMouse(this);
    m_pOptionsSubAudio = new COptionsSubAudio(this);
    m_pOptionsSubVideo = new COptionsSubVideo(this);

    if (!ModInfo().IsSinglePlayerOnly())
    {
        m_pOptionsSubVoice = new COptionsSubVoice(this);
    }

    m_pOptionsSubMiscellaneous = new OptionsSubMiscellaneous(this);

    AddPage(m_pOptionsSubMultiplayer, "#GameUI_Multiplayer");
    AddPage(m_pOptionsSubKeyboard, "#GameUI_Keyboard");
    AddPage(m_pOptionsSubMouse, "#GameUI_Mouse");
    AddPage(m_pOptionsSubAudio, "#GameUI_Audio");
    AddPage(m_pOptionsSubVideo, "#GameUI_Video");
    AddPage(m_pOptionsSubVoice, "#GameUI_Voice");
    AddPage(m_pOptionsSubMiscellaneous, "#GameUI_Miscellaneous");

    m_tabNames.Insert("multiplayer", m_pOptionsSubMultiplayer);
    m_tabNames.Insert("keyboard", m_pOptionsSubKeyboard);
    m_tabNames.Insert("mouse", m_pOptionsSubMouse);
    m_tabNames.Insert("audio", m_pOptionsSubAudio);
    m_tabNames.Insert("video", m_pOptionsSubVideo);
    m_tabNames.Insert("voice", m_pOptionsSubVoice);
    m_tabNames.Insert("miscellaneous", m_pOptionsSubMiscellaneous);

    SetApplyButtonVisible(true);
    GetPropertySheet()->SetTabWidth(84);
}

COptionsDialog::~COptionsDialog(void)
{
}

void COptionsDialog::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (!GameUI().IsInLevel() && code == vgui2::KEY_ESCAPE)
    {
        Close();
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void COptionsDialog::Activate(void)
{
    BaseClass::Activate();

    if (m_pOptionsSubMultiplayer)
    {
        if (GetActivePage() != m_pOptionsSubMultiplayer)
            GetPropertySheet()->SetActivePage(m_pOptionsSubMultiplayer);
    }
    else
    {
        if (GetActivePage() != m_pOptionsSubKeyboard)
            GetPropertySheet()->SetActivePage(m_pOptionsSubKeyboard);
    }
    ResetAllData();
    EnableApplyButton(false);
}

void COptionsDialog::OpenTab(const char* tabName) {
    int index = m_tabNames.Find(tabName);
    
    if(index != m_tabNames.InvalidIndex())
    {
        auto page = m_tabNames[index];
        if (GetActivePage() != page)
            GetPropertySheet()->SetActivePage(page);
    }
}

void COptionsDialog::Run(void)
{
    SetTitle("#GameUI_Options", true);
    Activate();
}

void COptionsDialog::OnClose(void)
{
    BaseClass::OnClose();
}

void COptionsDialog::OnGameUIHidden(void)
{
    for (int i = 0; i < GetChildCount(); i++)
    {
        Panel *pChild = GetChild(i);

        if (pChild)
            PostMessage(pChild, new KeyValues("GameUIHidden"));
    }
}
