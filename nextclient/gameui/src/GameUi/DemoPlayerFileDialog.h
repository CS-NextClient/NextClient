//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: DemoPlayerFileDialog.h: interface for the CDemoPlayerFileDialog class.
//
// $NoKeywords: $
//=============================================================================

#ifndef DEMOPLAYERFILEDIALOG_H
#define DEMOPLAYERFILEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CDemoPlayerFileDialog : public vgui2::Frame
{
public:
    CDemoPlayerFileDialog(vgui2::Panel *parent, const char *name);
    ~CDemoPlayerFileDialog();

protected:
    virtual void	OnClose();
    virtual void	OnCommand(const char *command);

private:
    typedef vgui2::Frame BaseClass;

    void LoadDemoList();
    vgui2::ListPanel *m_pDemoList;

};

#endif // !defined DEMOPLAYERFILEDIALOG_H
