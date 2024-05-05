//
//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: DemoPlayerDialog.cpp: implementation of the CDemoPlayerDialog class.
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include "DemoPlayerDialog.h"

#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <KeyValues.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/FileOpenDialog.h>

#include "GameUi.h"
#include "DemoPlayerFileDialog.h"
#include "DemoEventsDialog.h"

#include <basetypes.h>
#include <mathlib/mathlib.h>
#include <common.h>
#include <cvardef.h>
#include <pm_defs.h>
#include <kbutton.h>
#include <r_efx.h>
#include <r_studioint.h>

#include <custom.h>
#include <IWorld.h>
#include <IEngineWrapper.h>
#include <IDirector.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#undef PostMessage

#pragma warning(disable : 4244)	// 'conversion' conversion from 'type1' to 'type2', possible loss of data

using namespace vgui2;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoPlayerDialog::CDemoPlayerDialog(vgui2::Panel *parent) : Frame (parent, "DemoPlayerDialog")
{
    m_World = NULL;
    m_Engine = NULL;
    m_DemoPlayer = NULL;

    m_System = NULL;
//    m_Serial = 4001;	//HACK HACK, assuming we will never load more than 4000 modules
//    m_SystemTime = 0.0f;

    if ( !LoadModules() )
    {
        SetVisible( false );
        return;
    }

    if ( m_DemoPlayer->IsEditMode() )
    {
        SetBounds(0, 0, 464, 128);
    }
    else
    {
        SetBounds(0, 0, 464, 96);
    }

    SetSizeable( false );
    SetMoveable(true);

    vgui2::surface()->CreatePopup( GetVPanel(), false );

    SetVisible(true);

    SetTitle( "#GameUI_DemoPlayer", true );

    // main panel

    m_pLableTimeCode = new Label(this, "TimeLabel", "00:00:00");

    m_pTimeSlider = new Slider(this, "TimeSlider");
    m_pTimeSlider->SetRange( 0, 60 );
    m_pTimeSlider->AddActionSignalTarget( this );

//	Button *button;

    // Demo player buttons
    m_pButtonLoad = new Button(this, "LoadButton", "");
    m_pButtonPlay = new Button(this, "PlayButton", "");
    m_pButtonPause = new Button(this, "PauseButton", "");
    m_pButtonStepF = new Button(this, "StepFButton", "");
    m_pButtonStepB = new Button(this, "StepBButton", "");
    m_pButtonStart = new Button(this, "StartButton", "");
    m_pButtonEnd = new Button(this, "EndButton", "");
    m_pButtonSlower	= new Button(this, "SlowerButton", "");
    m_pButtonFaster	= new Button(this, "FasterButton", "");
    m_pButtonStop = new Button(this, "StopButton", "");

    // Demo editor buttons

    m_MasterButton = new ToggleButton(this, "MasterButton", "Master");
    m_MasterButton->AddActionSignalTarget( this );

    Button *button;
    button = new Button(this, "EventsButton", "Events");
    button = new Button(this, "SaveButton", "Save");

    m_NextTimeScale = 2.0f; // = x2
    m_lastSliderTime = -1;

    LoadControlSettings("Resource\\DemoPlayerDialog.res");
    LoadUserConfig("DemoPlayerDialog");

    m_hDemoEventsDialog = new CDemoEventsDialog( this, "DemoEventsDialog", m_Engine, m_DemoPlayer );
    m_hDemoEventsDialog->AddActionSignalTarget( this );

    m_hDemoPlayerFileDialog = NULL;
}

CDemoPlayerDialog::~CDemoPlayerDialog()
{
    if ( m_DemoPlayer )
    {
        // m_DemoPlayer->RemoveListener( this );  TODO m_DemoPlayer->RemoveListener( this );
    }
}

void CDemoPlayerDialog::ApplySchemeSettings( IScheme *pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    m_pButtonLoad->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_load", false), 0);
    m_pButtonPlay->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_play", false), 0);
    m_pButtonPause->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_pause", false), 0);
    m_pButtonStepF->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_stepf", false), 0);
    m_pButtonStepB->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_stepb", false), 0);
    m_pButtonStart->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_start", false), 0);
    m_pButtonEnd->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_end", false), 0);
    m_pButtonStop->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_stop", false), 0);
    m_pButtonFaster->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_faster", false), 0);
    m_pButtonSlower->SetImageAtIndex(0, scheme()->GetImage("Resource\\icon_slower", false), 0);
}

void CDemoPlayerDialog::OnThink()
{
    BaseClass::OnThink();

    if ( !m_DemoPlayer )
        return;

    if ( !m_DemoPlayer->IsActive() )
        return;

    Update();
}

void CDemoPlayerDialog::Update()
{
    // get total world time length

    if ( !m_DemoPlayer )
        return;

    if ( !m_DemoPlayer->IsActive() )
    {
        SetTitle( "Demo Player", false);
    }
    else if ( m_DemoPlayer->IsLoading() )
    {
        char title[300];
        sprintf( title, "Loading %s ...", m_DemoPlayer->GetFileName() );
        SetTitle( title, false);
    }
    else
    {
        SetTitle( m_DemoPlayer->GetFileName(), false);
    }

    frame_t * firstFrame = m_World->GetFirstFrame();
    frame_t * lastFrame = m_World->GetLastFrame();

    if ( firstFrame && lastFrame )
    {
        float range = lastFrame->time - firstFrame->time;
        m_pTimeSlider->SetRange(firstFrame->time,lastFrame->time) ;
        m_pTimeSlider->InvalidateLayout();
    }

    double	worldTime = m_DemoPlayer->GetWorldTime();
    float	timeScale = m_DemoPlayer->GetTimeScale();

    int sliderTime = m_pTimeSlider->GetValue();

    if ( sliderTime != m_lastSliderTime )
    {
        // slider has been moved by user, apply changes to editor
        m_DemoPlayer->SetWorldTime( sliderTime, false );
        if ( m_lastSliderTime > 0)
            OnPause();	// pause game while brwosing tru time
        m_lastSliderTime = sliderTime;
    }
    else
    {
        m_lastSliderTime = (int) worldTime;
        m_pTimeSlider->SetValue( m_lastSliderTime );
    }

    // set time code
    char timeCode[32]; // mins:secs:msecs
    std::string timeScaleString;
    std::string descrition = "Playing";

    if ( !m_DemoPlayer->IsActive() )
    {
        descrition = "Stopped";
    }
    else if ( m_DemoPlayer->IsPaused() )
    {
        descrition = "Paused";
    }

    if ( timeScale == 0.25f )
        timeScaleString ="x1/4";
    else if ( timeScale == 0.5f )
        timeScaleString ="x1/2";
    else if ( timeScale == 1.0f )
        timeScaleString ="x1";
    else if ( timeScale == 2.0f )
        timeScaleString ="x2";
    else if ( timeScale == 4.0f )
        timeScaleString ="x4";

    _snprintf(timeCode,31,"%02u:%02u:%02u  %s  %s", int(worldTime)/60, int(worldTime)%60, int(worldTime*100.0f)%100,
              timeScaleString.c_str(), descrition.c_str() );

    timeCode[31]=0;

    m_pLableTimeCode->SetText( timeCode );
}

void CDemoPlayerDialog::OnPlay()
{
    m_DemoPlayer->SetPaused( false );
    m_Engine->SetCvar("spec_autodirector", "1");	// enable director mode
}

bool CDemoPlayerDialog::LoadModules()
{
    m_System = SystemWrapper();

    if ( m_System == NULL )
    {
        return false;
    }

    m_Engine = (IEngineWrapper*)m_System->GetModule( "enginewrapper002", "", NULL );

    if ( m_Engine == NULL )
    {
        m_System->Printf("CDemoPlayerDialog::LoadModules: couldn't get engine interface.\n");
        return false;
    }

    m_DemoPlayer = (IDemoPlayer*) m_System->GetModule( DEMOPLAYER_INTERFACE_VERSION, "", NULL );

    if ( m_DemoPlayer == NULL )
    {
        m_System->Printf("CDemoPlayerDialog::LoadModules: couldn't load demo player module.\n");
        return false;
    }

    // m_DemoPlayer->RegisterListener( this ); TODO m_DemoPlayer->RegisterListener( this );

    m_World = m_DemoPlayer->GetWorld();

    if ( m_World == NULL )
    {
        m_System->Printf("CDemoPlayerDialog::LoadModules: couldn't get world module.\n");
        return false;
    }

    return true;
}

void CDemoPlayerDialog::OnPause()
{
    m_DemoPlayer->SetPaused( true );
    m_Engine->Cbuf_AddText("stopsound\n");	// Stop sound
}

void CDemoPlayerDialog::OnNextFrame(int direction )
{
    float time		= m_DemoPlayer->GetWorldTime();

    frame_t * frame = m_World->GetFrameByTime( time );

    if (!frame)
        return;

    frame = m_World->GetFrameBySeqNr( frame->seqnr + direction );

    if (!frame)
        return;

    m_DemoPlayer->SetWorldTime( frame->time, false );
    m_DemoPlayer->SetPaused( true );

}

void CDemoPlayerDialog::OnStart()
{
    frame_t * first = m_World->GetFirstFrame();

    if ( !first )
        return;

    m_DemoPlayer->SetWorldTime( first->time - 0.01f, false );
    m_DemoPlayer->SetTimeScale( 1.0f );
    m_DemoPlayer->SetPaused( true );
}

void CDemoPlayerDialog::OnSlower()
{
    float timeScale = m_DemoPlayer->GetTimeScale();

    if ( timeScale <= 0.25f )
        return;

    timeScale /= 2.0f;

    m_DemoPlayer->SetTimeScale( timeScale );
}

void CDemoPlayerDialog::OnSave()
{
    m_DemoPlayer->SaveGame( "demoedit.dem" );	// TODO, allow other name
}

void CDemoPlayerDialog::OnEvents()
{
    if ( !m_hDemoEventsDialog.Get() )
    {
        m_hDemoEventsDialog = new CDemoEventsDialog( this, "DemoEventsDialog", m_Engine, m_DemoPlayer );
        m_hDemoEventsDialog->AddActionSignalTarget( this );
    }

    m_hDemoEventsDialog->Activate();
    PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues("UpdateCmdList"));	// update event list

}

void CDemoPlayerDialog::OnFaster()
{
    float timeScale = m_DemoPlayer->GetTimeScale();

    if ( timeScale >= 4.0 )
        return;

    timeScale *= 2.0f;

    m_DemoPlayer->SetTimeScale( timeScale );
}

void CDemoPlayerDialog::OnEnd()
{
    frame_t * last = m_World->GetLastFrame();

    if ( !last )
        return;

    m_DemoPlayer->SetWorldTime( last->time, false );

    OnPause();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnClose()
{
    BaseClass::OnClose();
    MarkForDeletion();

    if ( m_DemoPlayer )
    {
        // m_DemoPlayer->RemoveListener( this ); TODO m_DemoPlayer->RemoveListener( this );
    }
}

void CDemoPlayerDialog::OnCommand( const char *command )
{
    if ( !m_DemoPlayer || !m_World || !m_Engine )
    {
        // don't do anything
        BaseClass::OnCommand(command);
        return;
    }

    if ( !strcmp( command, "pause" ) )
    {
        OnPause();
    }
    else if ( !strcmp( command, "load" ) )
    {
        OnLoad();
    }
    else if ( !strcmp( command, "play" ) )
    {
        OnPlay();
    }
    else if ( !strcmp( command, "stepf" ) )
    {
        OnNextFrame( 1 );
    }
    else if ( !strcmp( command, "stepb" ) )
    {
        OnNextFrame( -1 );
    }
    else if ( !strcmp( command, "start" ) )
    {
        OnStart();
    }
    else if ( !strcmp( command, "end" ) )
    {
        OnEnd();
    }
    else if ( !strcmp( command, "slower" ) )
    {
        OnSlower();
    }
    else if ( !strcmp( command, "faster" ) )
    {
        OnFaster();
    }
    else if ( !strcmp( command, "load" ) )
    {
        OnLoad();
    }
    else if ( !strcmp( command, "stop" ) )
    {
        OnStop();
    }
    else if ( !strcmp( command, "events" ) )
    {
        OnEvents();
    }
    else if ( !strcmp( command, "save" ) )
    {
        OnSave();
    }


    BaseClass::OnCommand(command);
}

void CDemoPlayerDialog::ReceiveSignal(ISystemModule * module, unsigned int signal)
{
    if ( !m_DemoPlayer )
        return;

    if ( (module->GetSerial() == m_DemoPlayer->GetSerial() ) )
    {
        switch ( signal )
        {
            case DIRECTOR_SIGNAL_UPDATE	:	if  ( m_DemoPlayer->IsEditMode() )

                    PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues("UpdateCmdList"));
                break;

            case DIRECTOR_SIGNAL_LASTCMD :	if  ( m_DemoPlayer->IsEditMode() )
                    PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues("UpdateLastCmd"));
                break;

            case DIRECTOR_SIGNAL_SHUTDOWN: m_DemoPlayer = NULL; break;	// don't use demo player anymore

            default : 	m_System->Printf("CDemoPlayerDialog::ReceiveSignal: unknown signal %i.\n", signal ); break;
        }
    }
}

void CDemoPlayerDialog::DemoSelected(const char* demoname)
{
    if ( !m_DemoPlayer )
        return;

    char fullstring[270];
    sprintf(fullstring, "viewdemo \"%s\"\n", demoname);

    m_DemoPlayer->Stop();
    m_Engine->Cbuf_AddText( fullstring );
}

void CDemoPlayerDialog::ButtonToggled(int state)
{
    if ( !m_DemoPlayer )
        return;

    m_DemoPlayer->SetMasterMode(state);
}

void CDemoPlayerDialog::OnStop()
{
    m_DemoPlayer->Stop();
    m_Engine->Cbuf_AddText( "stopdemo\n" );
    Update();
}

void CDemoPlayerDialog::OnLoad()
{
    if ( !m_hDemoPlayerFileDialog.Get() )
    {
        m_hDemoPlayerFileDialog = new CDemoPlayerFileDialog( this, "DemoPlayerFileDialog" );
        m_hDemoPlayerFileDialog->AddActionSignalTarget( this );
    }
    m_hDemoPlayerFileDialog->Activate();
}
