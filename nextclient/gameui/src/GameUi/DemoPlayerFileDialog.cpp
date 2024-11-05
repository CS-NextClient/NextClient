// DemoPlayerFileDialog.cpp: implementation of the CDemoPlayerFileDialog class.
//
//////////////////////////////////////////////////////////////////////

#include "DemoPlayerFileDialog.h"

using namespace vgui2;

#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <KeyValues.h>
#include <vgui_controls/ListPanel.h>

#include "FileSystem.h"
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoPlayerFileDialog::CDemoPlayerFileDialog(vgui2::Panel *parent, const char *name): Frame ( parent, name )
{
    SetBounds(0, 0, 278, 380);
    SetSizeable( false );

    surface()->CreatePopup( GetVPanel(), false );

    SetTitle( "#GameUI_LoadDemo", true );

    m_pDemoList = new ListPanel(this, "DemoList");

    Button * load = new Button(this, "LoadButton", "#GameUI_Load");
    Button * cancel = new Button(this, "CancelButton", "#GameUI_Cancel");

    LoadControlSettings("Resource/DemoPlayerFileDialog.res");

    m_pDemoList->AddColumnHeader(0, "demoname", "#GameUI_DemoFile", m_pDemoList->GetWide());

    LoadDemoList();
}

CDemoPlayerFileDialog::~CDemoPlayerFileDialog()
{

}

void CDemoPlayerFileDialog::LoadDemoList()
{
    // clear the current list (if any)
    m_pDemoList->DeleteAllItems();

    // iterate the filesystem getting the list of all the files
    // UNDONE: steam wants this done in a special way, need to support that
    FileFindHandle_t findHandle = NULL;
    const char *filename = g_pFullFileSystem->FindFirst("*.dem", &findHandle, "GAME");
    while (filename)
    {
        // add to the map list
        m_pDemoList->AddItem(new KeyValues("data", "demoname", filename), 0, false, false);

        // get the next file
        filename = g_pFullFileSystem->FindNext(findHandle);
    }

    g_pFullFileSystem->FindClose(findHandle);

    // set the first item to be selected
    if (m_pDemoList->GetItemCount() > 0)
    {
        int itemID = m_pDemoList->GetItemIDFromRow(0);
        m_pDemoList->SetSingleSelectedItem(itemID);
    }
}

void CDemoPlayerFileDialog::OnCommand( const char *command )
{
    if ( !strcmp( command, "load" ) )
    {
        // get first selected item
        int itemID = m_pDemoList->GetSelectedItem(0);
        if ( m_pDemoList->IsValidItemID(itemID) )
        {
            KeyValues *kv = m_pDemoList->GetItem(itemID);
            PostActionSignal(new KeyValues("DemoSelected", "demoname", kv->GetString("demoname", "") ));
        }

        OnClose();
    }

    BaseClass::OnCommand(command);
}

void CDemoPlayerFileDialog::OnClose()
{
    BaseClass::OnClose();
    MarkForDeletion();
}