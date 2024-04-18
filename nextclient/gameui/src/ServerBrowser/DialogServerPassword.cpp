//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "DialogServerPassword.h"

#include <KeyValues.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogServerPassword::CDialogServerPassword(Panel *parent) : Frame(parent, "DialogServerPassword")
{
    SetMinimumSize(320, 180);
    SetDeleteSelfOnClose(true);

    m_pInfoLabel = new Label(this, "InfoLabel", "This server requires a password to join.");
    m_pGameLabel = new Label(this, "GameLabel", "<game label>");
    m_pPasswordEntry = new TextEntry(this, "PasswordEntry");
    m_pConnectButton = new Button(this, "ConnectButton", "&Connect");
    m_pPasswordEntry->SetTextHidden(true);

    LoadControlSettings("servers\\DialogServerPassword.res");

    SetTitle("Server Requires Password - Servers", true);

    // set our initial position in the middle of the workspace
    MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogServerPassword::~CDialogServerPassword()
{
}

//-----------------------------------------------------------------------------
// Purpose: initializes the dialog and brings it to the foreground
//-----------------------------------------------------------------------------
void CDialogServerPassword::Activate(const char *serverName)
{
    BaseClass::Activate();

    m_pGameLabel->SetText(serverName);
    m_pConnectButton->SetAsDefaultButton(true);
    m_pPasswordEntry->RequestFocus();
}

void CDialogServerPassword::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (code == vgui2::KEY_ESCAPE)
    {
        OnCommand("Close");
    }
    else if (code == vgui2::KEY_ENTER)
    {
        OnCommand("Connect");
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CDialogServerPassword::OnCommand(const char *command)
{
    bool bClose = false;

    if (!stricmp(command, "Connect"))
    {
        KeyValues *msg = new KeyValues("JoinServerWithPassword");
        char buf[64];
        m_pPasswordEntry->GetText(buf, sizeof(buf)-1);
        msg->SetString("password", buf);
        PostActionSignal(msg);

        bClose = true;
    }
    else if (!stricmp(command, "Close"))
    {
        bClose = true;
    }
    else
    {
        BaseClass::OnCommand(command);
    }

    if (bClose)
    {
        PostMessage(this, new KeyValues("Close"));
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDialogServerPassword::PerformLayout()
{
    BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog on close
//-----------------------------------------------------------------------------
void CDialogServerPassword::OnClose()
{
    BaseClass::OnClose();
}
