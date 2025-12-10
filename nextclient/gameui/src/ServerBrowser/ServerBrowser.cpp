#include "ServerBrowser.h"
#include "ServerBrowserDialog.h"
#include "DialogGameInfo.h"

#undef CreateDialog
#undef PostMessage

#include <tier2/tier2.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

CServerBrowser g_ServerBrowserSingleton;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerBrowser, IServerBrowserEx, SERVERBROWSEREX_INTERFACE_VERSION, g_ServerBrowserSingleton);

CServerBrowser &ServerBrowser()
{
    return g_ServerBrowserSingleton;
}

CServerBrowser::CServerBrowser()
{
}

CServerBrowser::~CServerBrowser()
{
}

void CServerBrowser::CreateDialog()
{
    if (!server_browser_dialog_.Get())
    {
        server_browser_dialog_ = new CServerBrowserDialog(NULL);
        server_browser_dialog_->Initialize();
    }
}

bool CServerBrowser::Initialize(CreateInterfaceFn *factorylist, int factoryCount)
{
    g_pVGuiLocalize->AddFile(g_pFullFileSystem, "Servers/serverbrowser_%language%.txt");

    CreateDialog();
    return true;
}

void CServerBrowser::ActiveGameName(const char *szGameName, const char *szGameDir)
{
    vgui2::VPANEL panel = ServerBrowserDialog().GetVPanel();
    vgui2::ivgui()->PostMessage(panel, new KeyValues("ActiveGameName", "game", szGameName, "name", szGameDir), panel, 0.0f);
}

void CServerBrowser::ConnectToGame(int ip, int connectionport)
{
    vgui2::VPANEL panel = ServerBrowserDialog().GetVPanel();
    vgui2::ivgui()->PostMessage(panel, new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionport), panel, 0.0f);
}

void CServerBrowser::DisconnectFromGame()
{
    vgui2::VPANEL panel = ServerBrowserDialog().GetVPanel();
    vgui2::ivgui()->PostMessage(panel, new KeyValues("DisconnectedFromGame"), panel, 0.0f);
}

bool CServerBrowser::Activate()
{
    server_browser_dialog_->Open();
    return true;
}

bool CServerBrowser::Activate(ServerBrowserTab tab)
{
    server_browser_dialog_->Open();
    server_browser_dialog_->ActivateTab(tab);
    return true;
}

void CServerBrowser::Deactivate()
{
    if (server_browser_dialog_.Get())
        server_browser_dialog_->SaveUserData();
}

void CServerBrowser::Reactivate()
{
    if (server_browser_dialog_.Get())
    {
        server_browser_dialog_->LoadUserData();

        if (server_browser_dialog_->IsVisible())
            server_browser_dialog_->RefreshCurrentPage();
    }
}

vgui2::VPANEL CServerBrowser::GetPanel()
{
    return server_browser_dialog_.Get() ? server_browser_dialog_->GetVPanel() : NULL;
}

void CServerBrowser::SetParent(vgui2::VPANEL parent)
{
    if (server_browser_dialog_.Get())
        server_browser_dialog_->SetParent(parent);
}

void CServerBrowser::Shutdown()
{
    if (server_browser_dialog_.Get())
    {
        server_browser_dialog_->Close();
        server_browser_dialog_->MarkForDeletion();
    }
}

void CServerBrowser::CloseAllGameInfoDialogs()
{
    if (server_browser_dialog_.Get())
        server_browser_dialog_->CloseAllGameInfoDialogs();
}

bool CServerBrowser::JoinGame(unsigned int gameIP, unsigned int gamePort, const char *userName)
{
    server_browser_dialog_->JoinGame(gameIP, gamePort, userName);
    return true;
}
