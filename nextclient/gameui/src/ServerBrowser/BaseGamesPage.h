#ifndef BASEGAMESPAGE_H
#define BASEGAMESPAGE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>
#include "ServerList.h"
#include "IGameList.h"
#include "IServerRefreshResponse.h"
#include "serveritem.h"
#include "ServerListCompare.h"
#include <next_gameui/IGameUiNext.h>
#include <steam/steam_api.h>
#include <unordered_map>
#include <vector>

class CBaseGamesPage;

enum class GameListColumnType
{
    Password, // Use this column to sort according to the master server
    Bots,
    Secure,
    ServerName,
    ServerDesc,
    GameDesc,
    Players,
    Map,
    Ping,
    Ip,
    LastPlayed
};

class CGameListPanel : public vgui2::ListPanel
{
public:
    DECLARE_CLASS_SIMPLE(CGameListPanel, vgui2::ListPanel);

public:
    CGameListPanel(CBaseGamesPage *pOuter, const char *pName);
    [[nodiscard]] CBaseGamesPage* GetOuterGamesPage() const;

    // Panel
    void OnKeyCodeTyped(vgui2::KeyCode code) override;

private:
    CBaseGamesPage *m_pOuter;
};

class CBaseGamesPage : public vgui2::PropertyPage, public IServerRefreshResponse, public IGameList
{
    static const std::vector<GameListColumnType> DefaultColumns;

    DECLARE_CLASS_SIMPLE(CBaseGamesPage, vgui2::PropertyPage);

    const int kSecureFilterRowAll = 0;
    const int kSecureFilterRowSecure = 1;
    const int kSecureFilterRowUnSecure = 2;

protected:
    std::unordered_map<GameListColumnType, int> m_ColumnsMap;

public:
    CBaseGamesPage(vgui2::Panel *parent, const char *name, const char *pCustomResFilename = nullptr, const std::vector<GameListColumnType>& columns = DefaultColumns);
    ~CBaseGamesPage() override;

    // Panel
    void PerformLayout() override;
    void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
    void OnKeyCodeTyped(vgui2::KeyCode code) override;

    virtual MatchMakingKeyValuePair_t** GetFilter();
    virtual int GetFilterCount();
    virtual void SetRefreshing(bool state);
    virtual void LoadFilterSettings();
    virtual void UpdateDerivedLayouts();
    virtual bool OnGameListEnterPressed();

    virtual GuiConnectionSource GetConnectionSource() = 0;

    serveritem_t &GetServer(int serverID) override;

    int GetSelectedItemsCount();

    void GetFilterState(FilterState* out);

    MESSAGE_FUNC_INT(OnAddToFavorites, "AddToFavorites", serverID);

protected:
    void OnCommand(const char *command) override;
    void OnKeyCodePressed(vgui2::KeyCode code) override;
    virtual void OnSaveFilter(KeyValues *filter);
    virtual void OnLoadFilter(KeyValues *filter);
    //void OnPageShow() override;
    //void OnPageHide() override;
    void OnTick() override;

protected:
    MESSAGE_FUNC(OnItemSelected, "ItemSelected");
    MESSAGE_FUNC(OnBeginConnect, "ConnectToServer");
    MESSAGE_FUNC_INT_INT(ConnectedToGame, "ConnectedToGame", ip, connPort);
    MESSAGE_FUNC(DisconnectedFromGame, "DisconnectedFromGame");
    MESSAGE_FUNC(OnViewGameInfo, "ViewGameInfo");
    MESSAGE_FUNC_INT(OnRefreshServer, "RefreshServer", serverID);
    MESSAGE_FUNC_PTR_CHARPTR(OnTextChanged, "TextChanged", panel, text);
    MESSAGE_FUNC_PTR_INT(OnButtonToggled, "ButtonToggled", panel, state);


protected:
    virtual int GetRegionCodeToFilter() { return -1; }
    virtual bool CheckPrimaryFilters(serveritem_t &server);
    virtual bool CheckSecondaryFilters(serveritem_t &server);
    virtual void UpdateFilterSettings();
    virtual void CreateFilters();
    virtual void UpdateGameFilter();
    virtual bool IsActivated();

    void StartRefresh() override;
    //void GetNewServerList() override;
    void StopRefresh(CancelQueryReason reason) override;
    bool IsRefreshing() override;
    void ApplyFilters() override;
    int GetInvalidServerListID() override;

    // IServerRefreshResponse
    void ServerResponded(serveritem_t &server) override;
    void ServerFailedToRespond(serveritem_t &server) {};
    void RefreshComplete() {};

protected:
    void ApplyGameFilters();
    void UpdateRefreshStatusText();
    void ClearServerList();

protected:
    CServerList m_Servers;
    CGameListPanel *m_pGameList;
    vgui2::ComboBox *m_pLocationFilter{};

    vgui2::Button *m_pConnect;
    vgui2::Button *m_pRefreshAll;
    vgui2::Button *m_pRefreshQuick;
    vgui2::Button *m_pAddServer;
    vgui2::Button *m_pAddCurrentServer;
    vgui2::ToggleButton *m_pFilter{};

private:
    void ClearMasterFilter();
    void RecalculateMasterFilter();
    static std::wstring FormatUnixTime(const char* format, uint32_t unix_time);

private:
    const char *m_pCustomResFilename;

    vgui2::ComboBox *m_pGameFilter{};
    vgui2::TextEntry *m_pMapFilter{};
    vgui2::ComboBox *m_pPingFilter{};
    vgui2::ComboBox *m_pSecureFilter{};
    vgui2::CheckButton *m_pNoFullServersFilterCheck{};
    vgui2::CheckButton *m_pNoEmptyServersFilterCheck{};
    vgui2::CheckButton *m_pNoPasswordFilterCheck{};
    vgui2::CheckButton *m_pValidSteamAccountFilterCheck{};
    vgui2::Label *m_pFilterString{};
    char m_szComboAllText[64]{};

    bool m_bFiltersVisible{};
    vgui2::HFont m_hFont;

    int m_iPasswordImage{};
    int m_iBotImage{};
    int m_iSecureImage{};

    char m_szGameFilter[32]{};
    char m_szMapFilter[32]{};
    int m_iPingFilter;
    bool m_bFilterNoFullServers;
    bool m_bFilterNoEmptyServers;
    bool m_bFilterNoPasswordedServers;
    int m_iSelectedSecureFilterRow;
    int m_bFilterValidSteamAccount;

    enum
    {
        MAX_FILTER_KV_COUNT = 512
    };

    MatchMakingKeyValuePair_t* m_MasterFilter[MAX_FILTER_KV_COUNT]{};
    int m_iMasterFilterCount = 0;
};

#endif
