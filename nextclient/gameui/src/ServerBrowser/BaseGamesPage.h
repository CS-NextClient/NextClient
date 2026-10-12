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
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class CBaseGamesPage;
class CServerFilterComboBox;

// wide characters of the country filter text, terminator included
inline constexpr size_t kCountryFilterTextSize = 64;

enum class GameListColumnType
{
    Password, // Use this column to sort according to the master server
    Bots,
    Secure,
    Country,
    ServerName,
    GameMode,
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

private:
    CBaseGamesPage* m_pOuter;
    // country whose name the tooltip shows, empty for none
    char m_szTooltipCountryCode[kCountryCodeSize]{};

public:
    CGameListPanel(CBaseGamesPage *pOuter, const char *pName);
    [[nodiscard]] CBaseGamesPage* GetOuterGamesPage() const;

    // Panel
    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    // Keeps the tooltip on the name of the country whose flag is under the cursor
    void OnThink() override;
    void OnCursorExited() override;

    // ListPanel
    void RemoveAll() override;

private:
    void SetTooltipCountry(const char* country_code);
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
    // -1 for a column type the page does not show
    int GetColumnIndex(GameListColumnType type) const;
    // Drops the countries of the removed rows from the country filter
    void OnGameListCleared();

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
    void UpdateServerListItem(serveritem_t &server, bool sort_on_add);
    // Image list index of the country's flag, loading the flag image on first use; 0 for no flag
    int GetFlagImageIndex(const char* country_code);
    void RegisterCountry(const ServerDetailsNext& details);
    // also brings the country items up to the known countries
    void UpdateFilterCounts();
    void RebuildCountryFilterItems();
    // Adds the known countries the country filter lacks to its end, leaving the listed rows where they are
    void AddMissingCountryFilterItems();
    void AddCountryFilterItem(const std::wstring& label, const std::string& code);
    // The filters on what the server answers itself: players, ping, password, anti-cheat and map
    bool MatchesServerInfoFilters(const serveritem_t& server) const;
    bool MatchesGameModeFilter(const ServerDetailsNext& details) const;
    bool MatchesCountryFilter(const ServerDetailsNext& details) const;
    void ClearMasterFilter();
    void RecalculateMasterFilter();
    static std::wstring FormatUnixTime(const char* format, uint32_t unix_time);

private:
    const char *m_pCustomResFilename;

    vgui2::ComboBox *m_pGameFilter{};
    CServerFilterComboBox* m_pGameModeFilter{};
    CServerFilterComboBox* m_pCountryFilter{};
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

    vgui2::ImageList* m_pImageList{};
    // flag image indices by lower-case country code, and the codes in the order their flags were added
    std::unordered_map<std::string, int> m_FlagImages{};
    std::vector<std::string> m_FlagImageOrder{};

    char m_szGameFilter[32]{};
    char m_szMapFilter[32]{};
    int m_iPingFilter;
    bool m_bFilterNoFullServers;
    bool m_bFilterNoEmptyServers;
    bool m_bFilterNoPasswordedServers;
    int m_iSelectedSecureFilterRow;
    int m_bFilterValidSteamAccount;
    // game mode identifier to show, empty for all
    char m_szGameModeFilter[kGameModeIdSize]{};
    // country filter text in lower case, and the code of the listed country it names, empty when it names none
    wchar_t m_wszCountryFilter[kCountryFilterTextSize]{};
    char m_szCountryCodeFilter[kCountryCodeSize]{};
    // countries of the listed servers, code -> UTF-8 name
    std::map<std::string, std::string> m_KnownCountries{};
    bool m_bCountryFilterItemsStale{};
    // server list revision the filter counts were made from, whether a filter changed since, and the frame time from
    // which the next count may run, in seconds
    uint32_t m_iFilterCountsRevision{};
    bool m_bFilterCountsStale{};
    double m_flNextFilterCountsTime{};

    enum
    {
        MAX_FILTER_KV_COUNT = 512
    };

    MatchMakingKeyValuePair_t* m_MasterFilter[MAX_FILTER_KV_COUNT]{};
    int m_iMasterFilterCount = 0;
};

#endif
