//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DIALOGSERVERPASSWORD_H
#define DIALOGSERVERPASSWORD_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI2.h>
#include <vgui_controls/Frame.h>


namespace vgui
{
    class TextEntry;
    class Label;
    class Button;
};

//-----------------------------------------------------------------------------
// Purpose: Prompt for user to enter a password to be able to connect to the server
//-----------------------------------------------------------------------------
class CDialogServerPassword : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CDialogServerPassword, vgui2::Frame);

public:
    CDialogServerPassword(Panel *parent);
    ~CDialogServerPassword();

    // initializes the dialog and brings it to the foreground
    void Activate(const char *serverName);
    void OnKeyCodeTyped(vgui2::KeyCode code) override;


private:
    virtual void PerformLayout();
    virtual void OnCommand(const char *command);
    virtual void OnClose();

    vgui2::Label *m_pInfoLabel;
    vgui2::Label *m_pGameLabel;
    vgui2::TextEntry *m_pPasswordEntry;
    vgui2::Button *m_pConnectButton;
};


#endif // DIALOGSERVERPASSWORD_H
