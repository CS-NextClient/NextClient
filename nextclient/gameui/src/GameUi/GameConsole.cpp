//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "GameUi.h"
#include "GameConsole.h"
#include "GameConsoleNext.h"
#include "GameConsoleDialog.h"
#include "LoadingDialog.h"
#include <vgui/ISurfaceNext.h>

#include <KeyValues.h>
#include <vgui/Cursor.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static CGameConsole g_GameConsole;
//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameConsole &GameConsole()
{
    return g_GameConsole;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameConsole, IGameConsole, GAMECONSOLE_INTERFACE_VERSION_GS, g_GameConsole);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsole::CGameConsole()
{
    m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameConsole::~CGameConsole()
{
    m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: sets up the console for use
//-----------------------------------------------------------------------------
void CGameConsole::Initialize()
{
    if (m_bInitialized)
        return;

    m_pConsole = vgui2::SETUP_PANEL( new CGameConsoleDialog() ); // we add text before displaying this so set it up now!
    int swide, stall;
    //m_pConsole->SetParent(g_pTaskbar->GetVPanel());

    vgui2::surface()->GetScreenSize(swide, stall);
    int offset = 40;
    m_pConsole->SetBounds(
        offset, offset,
        std::min( swide - 2 * offset, 560 ), std::min( stall - 2 * offset, 400 ) );

    GameConsoleNext().Initialize(m_pConsole);
    m_bInitialized = true;

    engine->pfnAddCommand("condump", CGameConsole::OnCmdCondump);

    // This provides a 1 frame delay to display the text after the temporary buffer from the engine
    TaskCoro::RunInMainThread([this]
    {
        ExecuteTempConsoleBuffer();
    });
}

//-----------------------------------------------------------------------------
// Purpose: activates the console, makes it visible and brings it to the foreground
//-----------------------------------------------------------------------------
void CGameConsole::Activate()
{
    if (!m_bInitialized)
        return;

    if ( LoadingDialog() )
        return;

    vgui2::surface()->RestrictPaintToSinglePanel(NULL);
    m_pConsole->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: hides the console
//-----------------------------------------------------------------------------
void CGameConsole::Hide()
{
    if (!m_bInitialized)
        return;

    if (GameUI().IsInLevel())
        m_pConsole->SetFadeEffectDisableOverride(true);

    m_pConsole->Hide();

    if (GameUI().IsInLevel())
        m_pConsole->SetFadeEffectDisableOverride(false);
}

//-----------------------------------------------------------------------------
// Purpose: clears the console
//-----------------------------------------------------------------------------
void CGameConsole::Clear()
{
    if (!m_bInitialized)
        return;

    m_pConsole->Clear();
}

//-----------------------------------------------------------------------------
// Purpose: prints a message to the console
//-----------------------------------------------------------------------------
void CGameConsole::Printf(const char *format, ...)
{
    va_list argptr;
    char msg[4096];

        va_start(argptr, format);
    Q_vsnprintf(msg, sizeof(msg), format, argptr);
    msg[sizeof(msg) - 1] = 0;
        va_end(argptr);

    if (!m_bInitialized)
    {
        m_TempConsoleBuffer.emplace_back(msg, false);
    }
    else
    {
        m_pConsole->Print(msg);
    }
}

void CGameConsole::PrintfWithoutJsEvent(Color color, const std::wstring& msg)
{
    if (!m_bInitialized)
    {
        // I'm lazy
    }
    else
    {
        m_pConsole->ColorPrintWithoutJsEvent(color, msg.c_str());
    }
}

void CGameConsole::PrintfWithoutJsEvent(Color color, const std::string& msg)
{
    if (!m_bInitialized)
    {
        m_TempConsoleBuffer.emplace_back(msg, false);
    }
    else
    {
        m_pConsole->ColorPrintWithoutJsEvent(color, msg.c_str());
    }
}

void CGameConsole::ExecuteTempConsoleBuffer()
{
    for (const SavedMessageData& msg : m_TempConsoleBuffer)
    {
        if (msg.is_developer)
        {
            DPrintf("%s", msg.text.c_str());
        }
        else
        {
            Printf("%s", msg.text.c_str());
        }
    }

    m_TempConsoleBuffer.clear();
    m_TempConsoleBuffer.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: printes a debug message to the console
//-----------------------------------------------------------------------------
void CGameConsole::DPrintf(const char *format, ...)
{
    va_list argptr;
    char msg[4096];

        va_start(argptr, format);
    Q_vsnprintf(msg, sizeof(msg), format, argptr);
    msg[sizeof(msg) - 1] = 0;
        va_end(argptr);


    if (!m_bInitialized)
    {
        m_TempConsoleBuffer.emplace_back(msg, true);
    }
    else
    {
        m_pConsole->DPrint(msg);
    }
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently in focus
//-----------------------------------------------------------------------------
bool CGameConsole::IsConsoleVisible()
{
    if (!m_bInitialized)
        return false;

    bool is_visible = m_pConsole->IsVisible();
    return is_visible;
}

//-----------------------------------------------------------------------------
// Purpose: activates the console after a delay
//-----------------------------------------------------------------------------
void CGameConsole::ActivateDelayed(float time)
{
    if (!m_bInitialized)
        return;

    m_pConsole->PostMessage(m_pConsole, new KeyValues("Activate"), time);
}

void CGameConsole::SetParent(int parent)
{
    if (!m_bInitialized)
        return;

    m_pConsole->SetParent( static_cast<vgui2::VPANEL>( parent ));
}

//-----------------------------------------------------------------------------
// Purpose: static command handler
//-----------------------------------------------------------------------------
void CGameConsole::OnCmdCondump()
{
    g_GameConsole.m_pConsole->DumpConsoleTextToFile();
}
