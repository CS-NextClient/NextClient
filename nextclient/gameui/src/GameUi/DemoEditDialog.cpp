// DemoEditDialog.cpp: implementation of the CDemoEditDialog class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>


#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <KeyValues.h>
#include <vgui_controls/ComboBox.h>
#include <unordered_map>
#include <string>

#include <basetypes.h>
#include <mathlib/mathlib.h>
#include <common.h>
#include <cvardef.h>
#include <pm_defs.h>
#include <kbutton.h>
#include <r_efx.h>
#include <r_studioint.h>

#include "DemoEditDialog.h"

#include <IDirector.h>
#include <IDemoPlayer.h>
#include <IEngineWrapper.h>
#include <IObjectContainer.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui2;

#pragma warning(disable : 4244)	// 'conversion' conversion from 'type1' to 'type2', possible loss of data

extern char * COM_VarArgs(const char *format, ...);

typedef float vec_t;
typedef vec_t vec3_t[3];

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoEditDialog::CDemoEditDialog(vgui2::Panel *parent, const char *name, IEngineWrapper * engine,
                                 IDemoPlayer * demoPlayer ) : Frame ( parent, name )
{
    int		i;
    char	fieldName[16];

    m_DemoPlayer = demoPlayer;
    m_Engine = engine;

    m_ChangingData = false;

    SetBounds(0, 0, 336, 380);
    SetSizeable( false );

    surface()->CreatePopup( GetVPanel(), false );

    SetTitle( "Edit director commnd", true );

    m_TypeBox = new CLabeledCommandComboBox( this, "TypeBox" );

    for ( i = 0; i < DRC_CMD_LAST; i++)
    {
        m_TypeBox->AddItem( DirectorCmd::m_CMD_Name[i], DirectorCmd::m_CMD_Name[i] );
    }

    m_TypeBox->SetInitialItem( 0 );

    m_TimeStamp = new TextEntry( this, "TimeEntry" );

    Button * ok	= new Button(this, "OkButton", "Ok");
    Button * cancel	= new Button(this, "CancelButton", "Cancel");
    Button * gettime = new Button(this, "GetTimeButton", "Get time");
    m_GetViewButton = new Button(this, "GetViewButton", "Get view");

    int offset = 96;

    for ( i=0; i < MAX_DATA_FIELDS; i++)
    {
        sprintf( fieldName, "DataLabel%i", i );
        m_DataLabels[i] = new Label( this, fieldName, "" );
        m_DataLabels[i]->SetBounds( 10, offset ,96,24);

        sprintf( fieldName, "DataEntry%i", i );
        m_DataEntries[i] = new TextEntry( this, fieldName );
        m_DataEntries[i]->SetBounds( 104, offset, 224, 24);		// combination of SetPos/SetSize

        offset += 30;
    }

    LoadControlSettings("Resource/DemoEditDialog.res");

    m_DrcCmd.Clear();

    OnUpdate();
}

CDemoEditDialog::~CDemoEditDialog()
{

}

void CDemoEditDialog::SetDataField( int index, const char * label, const char * text)
{
    m_DataLabels[index]->SetText( label );
    m_DataLabels[index]->SetVisible( true );

    m_DataEntries[index]->SetText( text );
    m_DataEntries[index]->SetVisible( true );
}

int	CDemoEditDialog::GetDataFieldi( int index )
{
    int i;
    char	sz[1024];

    m_DataEntries[index]->GetText( sz, sizeof(sz) );

    sscanf( sz, "%i", &i );

    return i;
}

float CDemoEditDialog::GetDataFieldf( int index )
{
    float f;
    char	sz[1024];

    m_DataEntries[index]->GetText( sz, sizeof(sz) );

    sscanf( sz, "%f", &f );

    return f;
}

void CDemoEditDialog::GetDataField2f( int index, float * f1, float * f2 )
{
    char	sz[1024];

    m_DataEntries[index]->GetText( sz, sizeof(sz) );

    sscanf( sz, "%f %f", f1, f2  );
}

void CDemoEditDialog::GetDataField3f( int index, float * f1, float * f2, float * f3 )
{
    char	sz[1024];

    m_DataEntries[index]->GetText( sz, sizeof(sz) );

    sscanf( sz, "%f %f %f", f1, f2, f3  );
}

int	CDemoEditDialog::GetDataFieldx( int index )
{
    int i;
    char	sz[1024];

    m_DataEntries[index]->GetText( sz, sizeof(sz) );

    sscanf( sz, "%x", &i );

    return i;
}

char * CDemoEditDialog::GetDataFields( int index )
{
    static char	sz[1024];

    m_DataEntries[index]->GetText( sz, sizeof(sz) );

    return sz;
}

void CDemoEditDialog::OnOK()
{
    if ( !m_DemoPlayer )
    {
        BaseClass::OnCommand("Close");
        return;
    }

    UpdateDrcCmd();	// transfer data from text field to m_DrcCmd

    if ( m_DrcCmd.m_Index > 0 )
    {
        // this is an existing command, we have modified
        m_DemoPlayer->RemoveCommand( m_DrcCmd.m_Index );	// remove old command first
    }

    m_DemoPlayer->AddCommand( &m_DrcCmd );

    BaseClass::OnCommand("Close");
}

void CDemoEditDialog::OnGetView()
{
    if ( !m_Engine )
        return;

    if ( m_DrcCmd.GetType() != DRC_CMD_CAMERA )
        return;

    vec3_t	origin, angles;
    float	fov;
    int		flags;

    m_DrcCmd.GetCameraData(origin, angles, fov, flags );

    m_Engine->GetViewOrigin( origin );
    m_Engine->GetViewAngles( angles );

    m_DrcCmd.SetCameraData(origin, angles, fov, flags );

    OnUpdate();
}

void CDemoEditDialog::OnGetTime()
{
    if ( !m_DemoPlayer )
        return;

    m_DrcCmd.SetTime( m_DemoPlayer->GetWorldTime() );

    OnUpdate();
}

void CDemoEditDialog::UpdateDrcCmd()
{
    // get data from textfields
    char *	t;
    char	sz[1204];
    int		i1,i2,i3;
    float	f1,f2,f3,f4;
    vec3_t	v1,v2;

    m_TimeStamp->GetText( sz, sizeof(sz) );

    sscanf( sz, "%i:%i:%i", &i1, &i2, &i3 );

    m_DrcCmd.SetTime( (float)(i1*60)+(float)(i2)+(float)(i3)/100.0f ) ;

    switch ( m_DrcCmd.GetType() )
    {
        case DRC_CMD_EVENT		:	i1 = GetDataFieldi( 0 );	// Target 1
            i2 = GetDataFieldi( 1 );	// Target 2"
            i3 = GetDataFieldi( 2 );	// Flags
            m_DrcCmd.SetEventData( i1, i2, i3 );
            break;

        case DRC_CMD_MODE		:	i1 = GetDataFieldi( 0 ); // View Mode
            m_DrcCmd.SetModeData( i1 );
            break;

        case DRC_CMD_CAMERA		:	GetDataField3f( 0, &v1[0], &v1[1], &v1[2] );	// Position
            GetDataField3f( 1, &v2[0], &v2[1], &v2[2] );	// View Angle
            f1 = GetDataFieldf( 2 ); // FOV
            i1 = GetDataFieldi( 3 ); // Flags
            m_DrcCmd.SetCameraData( v1 ,v2, f1, i1);
            break;

        case DRC_CMD_TIMESCALE	:	f1 = GetDataFieldf( 0 );	//  Time Scale
            m_DrcCmd.SetTimeScaleData( f1 );
            break;

        case DRC_CMD_MESSAGE	:	i1 = GetDataFieldi( 0 ); // Effect
            i2 = GetDataFieldx( 1 ); // Color
            GetDataField2f( 2, &v1[0], & v1[1] ); // Position
            f1 = GetDataFieldf( 3 ); // Fade In
            f2 = GetDataFieldf( 4 ); // Fade Out"
            f3 = GetDataFieldf( 5 ); // Hold Time
            f4 = GetDataFieldf( 6 ); // FX Time
            t = GetDataFields( 7 ); // Text
            m_DrcCmd.SetMessageData( i1, i2, v1, f1, f2, f3, f4, t );
            break;

        case DRC_CMD_SOUND		:	t = GetDataFields( 0 ); // Name
            f1 = GetDataFieldf( 1 ); // Volume
            m_DrcCmd.SetSoundData( t, f1);
            break;

        case DRC_CMD_STATUS		:	m_DrcCmd.SetStatusData( 0, 0, 0 );
            break;

        case DRC_CMD_BANNER		:	t = GetDataFields( 0 ); // File
            m_DrcCmd.SetBannerData( t );
            break;

        case DRC_CMD_STUFFTEXT	:	t = GetDataFields( 0 ); // Command
            m_DrcCmd.SetStuffTextData( t );
            break;
    }





}

void CDemoEditDialog::OnUpdate()
{
    char	t[1024];
    int		i1,i2,i3;
    float	f1,f2,f3,f4;
    vec3_t	v1,v2;

    m_ChangingData = true;

    m_GetViewButton->SetVisible( false );

    ClearDataFields();	//  prophylactic

    int		type = m_DrcCmd.GetType();
    float	time = m_DrcCmd.GetTime();


    if ( type  <= DRC_CMD_NONE || type > DRC_CMD_LAST )
    {
        m_ChangingData = false;
        return;
    }

    m_TypeBox->ActivateItemByRow(type);

    sprintf( t, "%03u:%02u:%02u", (int)(time)/60, (int)(time)%60, int(time*100.0f)%100 );

    m_TimeStamp->SetText( t );

    switch ( type )
    {
        case DRC_CMD_EVENT		:	m_DrcCmd.GetEventData( i1, i2, i3 );
            SetDataField( 0, "Target 1", COM_VarArgs("%i",i1) );
            SetDataField( 1, "Target 2", COM_VarArgs("%i",i2) );
            SetDataField( 2, "Flags",    COM_VarArgs("%i",i3) );
            break;

        case DRC_CMD_MODE		:	m_DrcCmd.GetModeData( i1 );
            SetDataField( 0, "View Mode", COM_VarArgs("%i",i1) );
            break;

        case DRC_CMD_CAMERA		:	m_DrcCmd.GetCameraData( v1 ,v2, f1, i1);
            SetDataField( 0, "Position", COM_VarArgs("%.1f %.1f %.1f", v1[0], v1[1], v1[2] ) );
            SetDataField( 1, "View Angle", COM_VarArgs("%.1f %.1f %.1f",v2[0], v2[1], v2[2] ) );
            SetDataField( 2, "FOV",    COM_VarArgs("%.1f",f1) );
            SetDataField( 3, "Flags",    COM_VarArgs("%i",i1) );
            m_GetViewButton->SetVisible( true );
            break;

        case DRC_CMD_TIMESCALE	:	m_DrcCmd.GetTimeScaleData( f1 );
            SetDataField( 0, "Time Scale", COM_VarArgs("%.2f", f1) );
            break;

        case DRC_CMD_MESSAGE	:	m_DrcCmd.GetMessageData( i1, i2, v1, f1, f2, f3, f4, t);
            SetDataField( 0, "Effect", COM_VarArgs("%i", i1) );
            SetDataField( 1, "Color", COM_VarArgs("%x", i2) );
            SetDataField( 2, "Position", COM_VarArgs("%.2f %.2f", v1[0], v1[1]) );
            SetDataField( 3, "Fade In", COM_VarArgs("%.1f", f1) );
            SetDataField( 4, "Fade Out", COM_VarArgs("%.1f", f2) );
            SetDataField( 5, "Hold Time", COM_VarArgs("%.1f", f3) );
            SetDataField( 6, "FX Time", COM_VarArgs("%.1f", f4) );
            SetDataField( 7, "Text", t );
            break;

        case DRC_CMD_SOUND		:	m_DrcCmd.GetSoundData( t, f1);
            SetDataField( 0, "Name", t );
            SetDataField( 1, "Volume", COM_VarArgs("%.2f", f1) );
            break;

        case DRC_CMD_STATUS		:	m_DrcCmd.GetStatusData( i1, i2, i3 );
            break;

        case DRC_CMD_BANNER		:	m_DrcCmd.GetBannerData( t );
            SetDataField( 0, "File", t );
            break;

        case DRC_CMD_STUFFTEXT	:	m_DrcCmd.GetStuffTextData( t );
            SetDataField( 0, "Command", t );
            break;
    }

    m_ChangingData = false;
}

void CDemoEditDialog::ClearDataFields()
{
    for ( int i=0; i < MAX_DATA_FIELDS; i++)
    {
        m_DataLabels[i]->SetText("");
        m_DataLabels[i]->SetVisible( false );

        m_DataEntries[i]->SetText("");
        m_DataEntries[i]->SetVisible( false );
    }
}

void CDemoEditDialog::SetCommand(DirectorCmd * cmd)
{
    m_DrcCmd.Copy( cmd );
    OnUpdate();
}

void CDemoEditDialog::OnCommand( const char *command )
{
    if ( !strcmp( command, "ok" ) )
    {
        OnOK();
    }
    else if ( !strcmp( command, "getview" ) )
    {
        OnGetView();
    }
    else if ( !strcmp( command, "gettime" ) )
    {
        OnGetTime();
    }

    BaseClass::OnCommand(command);
}

void CDemoEditDialog::OnTypeChanged()
{
    // user changed type in combo box

    if ( m_ChangingData )
        return;	// don't react on own changes done by code, only user input

    // keep command time & index
    float time = m_DrcCmd.m_Time;
    int	index = m_DrcCmd.m_Index;

    m_DrcCmd.Clear();

    m_DrcCmd.m_Time = time;
    m_DrcCmd.m_Index = index;

    vec3_t v;
    char type[256];
    m_TypeBox->GetText( type, 256);

    for (int i=0; i < DRC_CMD_LAST; i++)
    {
        if ( !strcmp(type, DirectorCmd::m_CMD_Name[i] ) )
        {
            m_DrcCmd.m_Type = i;
            break;
        }
    }

    switch ( m_DrcCmd.m_Type )
    {
        case DRC_CMD_EVENT		:	m_DrcCmd.SetEventData( 1, 0, 0 );
            break;

        case DRC_CMD_MODE		:	m_DrcCmd.SetModeData( 0 );
            break;

        case DRC_CMD_CAMERA		:	v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f;

            m_DrcCmd.SetCameraData( v ,v, 90, 0 );
            break;

        case DRC_CMD_TIMESCALE	:	m_DrcCmd.SetTimeScaleData( 1.0f );
            break;

        case DRC_CMD_MESSAGE	:	v[0] = 0.5f; v[1] = 0.9f; v[2] = 0.0f;
            m_DrcCmd.SetMessageData( 0, 0x0000ff, v, 1.0, 1.0, 5.0, 0.0, "Add your message here");
            break;

        case DRC_CMD_SOUND		:	m_DrcCmd.SetSoundData( "barney/heybuddy.wav", 1.0f );
            break;

        case DRC_CMD_STATUS		:	m_DrcCmd.SetStatusData( 0, 0, 0 );
            break;

        case DRC_CMD_BANNER		:	m_DrcCmd.SetBannerData( "gfx/temp/file.tga" );
            break;

        case DRC_CMD_STUFFTEXT	:	m_DrcCmd.SetStuffTextData( "echo Enter command here");
            break;
    }

    OnUpdate();

}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CDemoEditDialog::m_MessageMap[] =
    {
        MAP_MESSAGE( CDemoEditDialog, "ControlModified", OnTypeChanged ),
    };

IMPLEMENT_PANELMAP(CDemoEditDialog, BaseClass);