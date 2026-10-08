#include "ServerListCompare.h"
#include "serveritem.h"
#include "ServerBrowserDialog.h"
#include "BaseGamesPage.h"
#include "ServerBrowser/ServerBrowserText.h"
#include "ServerBrowser/ServerGameModeNames.h"
#include <GameServerHelpers.h>

#include <string>

#include <nitro_utils/string_utils.h>

#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/ListPanel.h>

namespace
{
    // The mode's list cell text as the list shows it, with a display name token resolved
    std::wstring GetGameModeCellDisplayText(const char* mode)
    {
        const char* text = ServerGameMode_GetCellText(mode);

        if (text[0] == '#')
        {
            const wchar_t* localized = g_pVGuiLocalize->Find(text);

            if (localized != nullptr)
            {
                return localized;
            }
        }

        return nitro_utils::utf8_to_wide(text);
    }
} // namespace

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

    int s1p = GetHumanPlayerCount(s1.gs);
    int s1m = std::max(0, s1.gs.m_nMaxPlayers - s1.gs.m_nBotPlayers);
    int s2p = GetHumanPlayerCount(s2.gs);
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

int __cdecl CountryCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
    CGameListPanel* game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);

    if (!game_list_panel)
    {
        return 0;
    }

    serveritem_t& s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t& s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    return ServerBrowserText_CompareUnknownLast(s1.next_details.country_code, s2.next_details.country_code);
}

int __cdecl GameModeCompare(ListPanel* pPanel, const ListPanelItem& p1, const ListPanelItem& p2)
{
    CGameListPanel* game_list_panel = dynamic_cast<CGameListPanel*>(pPanel);

    if (!game_list_panel)
    {
        return 0;
    }

    serveritem_t& s1 = game_list_panel->GetOuterGamesPage()->GetServer(p1.userData);
    serveritem_t& s2 = game_list_panel->GetOuterGamesPage()->GetServer(p2.userData);

    std::wstring mode1 = GetGameModeCellDisplayText(s1.next_details.game_mode);
    std::wstring mode2 = GetGameModeCellDisplayText(s2.next_details.game_mode);

    return ServerBrowserText_CompareUnknownLast(mode1.c_str(), mode2.c_str());
}
