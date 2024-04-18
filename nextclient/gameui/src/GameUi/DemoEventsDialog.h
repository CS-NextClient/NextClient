// DemoEventsDialog.h: interface for the CDemoEventsDialog class.
//
//////////////////////////////////////////////////////////////////////

#if !defined DEMOEVENTSDIALOG_H
#define DEMOEVENTSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <common/DirectorCmd.h>
#include "DemoEditDialog.h"

class IDemoPlayer;
class IDirector;
class IEngineWrapper;

class CDemoEventsDialog : public vgui2::Frame
{

public:
    CDemoEventsDialog(vgui2::Panel *parent, const char *name, IEngineWrapper * engine,
                      IDemoPlayer * demoPlayer );
    virtual ~CDemoEventsDialog();

    typedef vgui2::Frame BaseClass;

protected:
    // virtual overrides

    virtual void	OnCommand(const char *command);
    // virtual void	OnMessage(vgui2::KeyValues *params, vgui2::VPANEL fromPanel);

    void			OnUpdate();	// update director command list
    void			GetCurrentCmd();
    void			OnAdd();
    void			OnGoto();
    void			OnRemove();
    void			OnModify();

    vgui2::ListPanel				*m_EventList;
    IEngineWrapper				*m_Engine;
    IDemoPlayer					*m_DemoPlayer;
    DirectorCmd					*m_CurrentCmd;
    CDemoEditDialog				*m_EditDialog;

    DECLARE_PANELMAP();

};

#endif // !defined DEMOEVENTSDIALOG_H
