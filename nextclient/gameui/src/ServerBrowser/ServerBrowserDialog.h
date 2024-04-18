#ifndef SERVERBROWSERDIALOG_H
#define SERVERBROWSERDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#pragma warning(disable: 4355)

#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/PHandle.h>

#include "utlvector.h"
#include "IGameList.h"
#include "serveritem.h"
#include "UniqueGames.h"
#include <next_gameui/IGameUiNext.h>
#include "../IServerBrowserEx.h"

#include <steam/steam_api.h>

#include "FriendsGames.h"

class CServerContextMenu;

class CFavoriteGames;
class CInternetGames;
class CSpectateGames;
class CLanGames;
class CHistoryGames;
class CDialogGameInfo;
class CBaseGamesPage;

class CServerBrowserDialog : public vgui2::Frame
{
    DECLARE_CLASS_SIMPLE(CServerBrowserDialog, vgui2::Frame);

    const char* kUserSaveDataPath = "ServerBrowser.vdf";

public:
    static CServerBrowserDialog *GetInstance();

    explicit CServerBrowserDialog(vgui2::Panel *parent);
    virtual ~CServerBrowserDialog();

    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OnCommand(const char* command) override;

    void Initialize();
    void Open();
    void LoadUserData();
    void ActivateTab(ServerBrowserTab tab);
    void SaveUserData();
    KeyValues *GetFilterSaveData(const char *filterSet);

    Panel *GetActivePage();
    void RefreshCurrentPage();
    void UpdateStatusText(const char *format, ...);
    void UpdateStatusText(wchar_t *unicode);

    CServerContextMenu *GetContextMenu(vgui2::Panel *pParent);
    CDialogGameInfo *OpenGameInfoDialog(IGameList *gameList, unsigned int serverIndex);
    CDialogGameInfo *OpenGameInfoDialog(uint32 serverIP, uint16 serverPort, const char *titleName);
    void CloseAllGameInfoDialogs();

    const char *GetActiveModName();
    const char *GetActiveGameName();
    serveritem_t &GetServer(unsigned int serverID);
    void AddServerToFavorites(uint32_t ip, uint16_t port);
    void AddServerToFavorites(const gameserveritem_t& serveritem);
    CDialogGameInfo *JoinGame(IGameList *gameList, unsigned int serverIndex, GuiConnectionSource connection_source = GuiConnectionSource::Unknown);
    CDialogGameInfo *JoinGame(uint32 serverIP, uint16 serverPort, const char *titleName);
    servernetadr_t &GetCurrentConnectedServer() { return m_CurrentConnection; }

    void GetInternetFilterState(FilterState* out);

    static void GetMostCommonQueryPorts(CUtlVector<uint16> &ports);

private:
    void ReloadFilterSettings();

    bool GetDefaultScreenPosition(int &x, int &y, int &wide, int &tall) override;
    void ActivateBuildMode() override;

    MESSAGE_FUNC(OnGameListChanged, "PageChanged");
    MESSAGE_FUNC_PARAMS(OnActiveGameName, "ActiveGameName", name);
    MESSAGE_FUNC_PARAMS(OnConnectToGame, "ConnectedToGame", kv);
    MESSAGE_FUNC(OnDisconnectFromGame, "DisconnectedFromGame");

private:
    CUtlVector<vgui2::DHANDLE<CDialogGameInfo>> m_GameInfoDialogs;
    IGameList *m_pGameList;

    vgui2::Label *m_pStatusLabel;

    vgui2::PropertySheet *m_pTabPanel;
    CFavoriteGames *m_pFavorites;
    CHistoryGames *m_pHistory;
    CInternetGames *m_pInternetGames;
    CUniqueGames* m_pUniqueGames;
    //CSpectateGames *m_pSpectateGames;
    CLanGames *m_pLanGames;
    CFriendsGames *m_pFriendsGames;

    KeyValues *m_pSavedData;
    KeyValues *m_pFilterData;

    CServerContextMenu *m_pContextMenu;

    char m_szGameName[128];
    char m_szModDir[128];
    int m_iLimitAppID;

    bool m_bCurrentlyConnected;
    servernetadr_t m_CurrentConnection;
};

extern CServerBrowserDialog &ServerBrowserDialog();

#endif