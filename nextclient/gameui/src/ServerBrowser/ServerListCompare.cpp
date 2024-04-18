#include "ServerListCompare.h"
#include "serveritem.h"
#include "ServerBrowserDialog.h"
#include "BaseGamesPage.h"

#include <KeyValues.h>
#include <vgui_controls/ListPanel.h>

int __cdecl ServerIdCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    if (s1.serverID < s2.serverID)
        return -1;
    else if (s1.serverID > s2.serverID)
        return 1;

    return 0;
}

int __cdecl PasswordCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    if (s1.gs.m_bPassword < s2.gs.m_bPassword)
        return 1;
    else if (s1.gs.m_bPassword > s2.gs.m_bPassword)
        return -1;

    return 0;
}

int __cdecl BotsCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    if (s1.gs.m_nBotPlayers < s2.gs.m_nBotPlayers)
        return 1;
    else if (s1.gs.m_nBotPlayers > s2.gs.m_nBotPlayers)
        return -1;

    return 0;
}

int __cdecl SecureCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    if (s1.gs.m_bSecure < s2.gs.m_bSecure)
        return 1;
    else if (s1.gs.m_bSecure > s2.gs.m_bSecure)
        return -1;

    return 0;
}

int __cdecl PingCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    if (s1.gs.m_nPing < s2.gs.m_nPing)
        return -1;
    else if (s1.gs.m_nPing > s2.gs.m_nPing)
        return 1;

    return 0;
}

int __cdecl MapCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    return Q_stricmp(s1.gs.m_szMap, s2.gs.m_szMap);
}

int __cdecl GameCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    return Q_stricmp(s1.gs.m_szGameDescription, s2.gs.m_szGameDescription);
}

int __cdecl ServerNameCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    return Q_stricmp(s1.gs.GetName().c_str(), s2.gs.GetName().c_str());
}

int __cdecl PlayersCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    int s1p = std::max(0, s1.gs.m_nPlayers - s1.gs.m_nBotPlayers);
    int s1m = std::max(0, s1.gs.m_nMaxPlayers - s1.gs.m_nBotPlayers);
    int s2p = std::max(0, s2.gs.m_nPlayers - s2.gs.m_nBotPlayers);
    int s2m = std::max(0, s2.gs.m_nMaxPlayers - s2.gs.m_nBotPlayers);

    if (s1p > s2p)
        return -1;

    if (s1p < s2p)
        return 1;

    if (s1m > s2m)
        return -1;

    if (s1m < s2m)
        return 1;

    return 0;
}

int __cdecl LastPlayedCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    auto game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);
    if (!game_list_panel)
        return 0;

    serveritem_t &s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t &s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    if (s1.gs.m_ulTimeLastPlayed < s2.gs.m_ulTimeLastPlayed)
        return -1;
    else if (s1.gs.m_ulTimeLastPlayed > s2.gs.m_ulTimeLastPlayed)
        return 1;

    return 0;
}