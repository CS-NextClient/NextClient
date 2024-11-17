// DemoEventsDialog.cpp: implementation of the CDemoEventsDialog class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>

#include "DemoEventsDialog.h"


#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <KeyValues.h>
#include <vgui_controls/ListPanel.h>

#include <basetypes.h>
#include <mathlib/mathlib.h>
#include <common.h>
#include <cvardef.h>
#include <pm_defs.h>
#include <kbutton.h>
#include <r_efx.h>
#include <r_studioint.h>

#include "DemoEditDialog.h"

#include <ISystemModule.h>
#include <IDemoPlayer.h>
#include <IEngineWrapper.h>
#include <IObjectContainer.h>
#include <IDirector.h>
#include <common/DirectorCmd.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui2;

#pragma warning(disable : 4244)	// 'conversion' conversion from 'type1' to 'type2', possible loss of data

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoEventsDialog::CDemoEventsDialog(vgui2::Panel *parent, const char *name, IEngineWrapper * engine,
                                     IDemoPlayer * demoPlayer ) : Frame ( parent, name )
{
    m_DemoPlayer = demoPlayer;
    m_Engine = engine;

    SetBounds(0, 0, 464, 380);
    SetSizeable( false );

    surface()->CreatePopup( GetVPanel(), false );

    SetTitle( "Event List", true );

    m_EventList = new ListPanel(this, "EventList");

    Button * add	= new Button(this, "AddButton",	"Add");
    Button * remove = new Button(this, "RemoveButton", "Remove");
    Button * modify = new Button(this, "ModifyButton", "Modify");
    Button * goto_	= new Button(this, "GotoButton", "Goto");


    LoadControlSettings("Resource/DemoEventsDialog.res");

    m_EventList->AddColumnHeader(0, "time", "Time", 32, true);
    m_EventList->AddColumnHeader(1, "type", "Type", 128, true);

    m_CurrentCmd = NULL;

    m_EditDialog = new CDemoEditDialog( this, "DemoEditDialog", m_Engine, m_DemoPlayer );
    m_EditDialog->Activate();
}

CDemoEventsDialog::~CDemoEventsDialog()
{

}

void CDemoEventsDialog::OnCommand( const char *command )
{
    if ( !m_DemoPlayer || !m_Engine )
    {
        // don't do anything
        BaseClass::OnCommand(command);
        return;
    }

    GetCurrentCmd();

    if ( !strcmp( command, "goto" ) )
    {
        OnGoto();
    }
    else if ( !strcmp( command, "remove" ) )
    {
        OnRemove();
    }
    else if ( !strcmp( command, "modify" ) )
    {
        OnModify();
    }
    else if ( !strcmp( command, "add" ) )
    {
        OnAdd();
    }

    BaseClass::OnCommand(command);
}

void CDemoEventsDialog::GetCurrentCmd()
{
    m_CurrentCmd = NULL;

    // get first selected item
    int itemID = m_EventList->GetSelectedItem( 0 );
    if ( itemID < 0)
        return;

    int row = m_EventList->GetItemCurrentRow(itemID);
    int index = row + 1;

    IObjectContainer * cmdList = m_DemoPlayer->GetCommands();

    if ( !cmdList )
        return;

    DirectorCmd * cmd = (DirectorCmd *) cmdList->GetFirst();

    while ( cmd )
    {
        if ( cmd->m_Index == index )
        {
            m_CurrentCmd = cmd;
            return;
        }

        cmd = (DirectorCmd *) cmdList->GetNext();
    }
}

void CDemoEventsDialog::OnAdd()
{
    DirectorCmd  newcmd;

    vec3_t	origin, angles;

    m_DemoPlayer->SetPaused( true );

    m_Engine->GetViewOrigin( origin );
    m_Engine->GetViewAngles( angles );

    newcmd.m_Type = DRC_CMD_CAMERA;
    newcmd.Clear();
    newcmd.m_Time = m_DemoPlayer->GetWorldTime();
    newcmd.SetCameraData( origin, angles, 90.0f, 0);	// camera event as default
    newcmd.m_Index = -1;	// mark as new command

    m_EditDialog->SetCommand( &newcmd );
    m_EditDialog->Activate();
}

void CDemoEventsDialog::OnGoto()
{
    if ( !m_CurrentCmd )
        return;

    m_DemoPlayer->SetWorldTime( m_CurrentCmd->m_Time, false );
    m_DemoPlayer->ExecuteDirectorCmd( m_CurrentCmd );
    m_DemoPlayer->SetPaused( true );
}

void CDemoEventsDialog::OnModify()
{
    if ( !m_CurrentCmd )
        return;

    m_EditDialog->SetCommand( m_CurrentCmd );
    m_EditDialog->Activate();
}

void CDemoEventsDialog::OnRemove()
{
    if ( !m_CurrentCmd )
        return;

    m_DemoPlayer->RemoveCommand( m_CurrentCmd->m_Index );
}

void CDemoEventsDialog::OnUpdate()
{
    m_EventList->DeleteAllItems();

    IObjectContainer * cmdList = m_DemoPlayer->GetCommands();

    if ( !cmdList )
        return;

    DirectorCmd * cmd = (DirectorCmd *) cmdList->GetFirst();

    while ( cmd )
    {
        char time[32];
        char index[8];

        sprintf(time, "%03u:%02u:%02u", (int)(cmd->m_Time)/60, (int)(cmd->m_Time)%60, int(cmd->m_Time*100.0f)%100 );
        sprintf(index, "%i", cmd->m_Index);


        KeyValues * entry = new KeyValues( index, "time", time, "type", cmd->ToString() );

        // add to the map list
        m_EventList->AddItem( entry, 0, false, false );

        cmd = (DirectorCmd *) cmdList->GetNext();
    }

    m_CurrentCmd = m_DemoPlayer->GetLastCommand();

    if ( m_CurrentCmd )
    {
        int itemID = m_EventList->GetItemIDFromRow(m_CurrentCmd->m_Index-1);
        m_EventList->SetSingleSelectedItem(itemID);
    }
    else
    {
        int itemID = m_EventList->GetItemIDFromRow(0);
        m_EventList->SetSingleSelectedItem( itemID );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CDemoEventsDialog::m_MessageMap[] =
    {
        MAP_MESSAGE( CDemoEventsDialog, "UpdateCmdList", OnUpdate),
        MAP_MESSAGE( CDemoEventsDialog, "UpdateLastCmd", OnUpdate),
    };

IMPLEMENT_PANELMAP(CDemoEventsDialog, BaseClass);