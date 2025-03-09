#include "ServerBrowser.h"
#include "ServerBrowserDialog.h"
#include "DialogGameInfo.h"

#include <tier2/tier2.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

#undef CreateDialog

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
    if (!m_hInternetDlg.Get())
    {
        m_hInternetDlg = new CServerBrowserDialog(NULL);
        m_hInternetDlg->Initialize();
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
    vgui2::ivgui()->PostMessage(panel, new KeyValues("ActiveGameName", "game", szGameName, "name", szGameDir), panel, 0.0);
}

void CServerBrowser::ConnectToGame(int ip, int connectionport)
{
    vgui2::VPANEL panel = ServerBrowserDialog().GetVPanel();
    vgui2::ivgui()->PostMessage(panel, new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionport), panel, 0.0);
}

void CServerBrowser::DisconnectFromGame()
{
    vgui2::VPANEL panel = ServerBrowserDialog().GetVPanel();
    vgui2::ivgui()->PostMessage(panel, new KeyValues("DisconnectedFromGame"), panel, 0.0);
}

bool CServerBrowser::Activate()
{
    m_hInternetDlg->Open();
    return true;
}

bool CServerBrowser::Activate(ServerBrowserTab tab)
{
    m_hInternetDlg->Open();
    m_hInternetDlg->ActivateTab(tab);
    return true;
}

void CServerBrowser::Deactivate()
{
    if (m_hInternetDlg.Get())
        m_hInternetDlg->SaveUserData();
}

void CServerBrowser::Reactivate()
{
    if (m_hInternetDlg.Get())
    {
        m_hInternetDlg->LoadUserData();

        if (m_hInternetDlg->IsVisible())
            m_hInternetDlg->RefreshCurrentPage();
    }
}

vgui2::VPANEL CServerBrowser::GetPanel()
{
    return m_hInternetDlg.Get() ? m_hInternetDlg->GetVPanel() : NULL;
}

void CServerBrowser::SetParent(vgui2::VPANEL parent)
{
    if (m_hInternetDlg.Get())
        m_hInternetDlg->SetParent(parent);
}

void CServerBrowser::Shutdown()
{
    if (m_hInternetDlg.Get())
    {
        m_hInternetDlg->Close();
        m_hInternetDlg->MarkForDeletion();
    }
}

void CServerBrowser::CloseAllGameInfoDialogs()
{
    if (m_hInternetDlg.Get())
        m_hInternetDlg->CloseAllGameInfoDialogs();
}

bool CServerBrowser::JoinGame(unsigned int gameIP, unsigned int gamePort, const char *userName)
{
    m_hInternetDlg->JoinGame(gameIP, gamePort, userName);
    return true;
}
