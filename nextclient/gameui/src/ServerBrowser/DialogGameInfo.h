#ifndef DIALOGGAMEINFO_H
#define DIALOGGAMEINFO_H

#ifdef _WIN32
#pragma once
#endif

#include <steam/steam_api.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>

class CDialogGameInfo : public vgui2::Frame, public ISteamMatchmakingPlayersResponse, public ISteamMatchmakingPingResponse
{
    DECLARE_CLASS_SIMPLE(CDialogGameInfo, vgui2::Frame);

public:
    CDialogGameInfo(vgui2::Panel *parent, uint32 ip, uint16 port);
    ~CDialogGameInfo() override;

    void Run(const char *titleName);
    void Connect();

    servernetadr_t GetAddress();

protected:
    MESSAGE_FUNC(OnConnect, "Connect");
    MESSAGE_FUNC(OnRefresh, "Refresh");
    MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel);
    MESSAGE_FUNC_PTR(OnRadioButtonChecked, "RadioButtonChecked", panel) { OnButtonToggled(panel); }

    MESSAGE_FUNC_CHARPTR(OnJoinServerWithPassword, "JoinServerWithPassword", password);
    MESSAGE_FUNC_INT_INT(OnConnectToGame, "ConnectedToGame", ip, port);

    // vgui2::Frame
    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OnTick() override;
    void PerformLayout() override;

    // ISteamMatchmakingPlayersResponse
    void AddPlayerToList(const char *playerName, int score, float timePlayedSeconds) override;
    void PlayersFailedToRespond() override;
    void PlayersRefreshComplete() override;

    // ISteamMatchmakingPingResponse
    void ServerResponded(gameserveritem_t &server) override;
    void ServerFailedToRespond() override;

private:
    void ShowAutoRetryOptions(bool state);

    void SendPlayerQuery();
    void CancelPlayerQuery();

    void SendPingQueryIfNotAny();
    void CancelPingQuery();

    void ClearPlayerList();

    bool ConnectToServer();
    void ApplyConnectCommand(const gameserveritem_t &server);

    static int PlayerTimeColumnSortFunc(vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2);

private:
    long m_iRequestRetry;

    vgui2::Button *m_pConnectButton;
    vgui2::Button *m_pCloseButton;
    vgui2::Button *m_pRefreshButton;
    vgui2::Label *m_pInfoLabel;
    vgui2::ToggleButton *m_pAutoRetry;
    vgui2::RadioButton *m_pAutoRetryAlert;
    vgui2::RadioButton *m_pAutoRetryJoin;
    vgui2::ListPanel *m_pPlayerList;

    uint32 server_ip_;
    uint16 server_port_;

    HServerQuery players_server_query_{};
    HServerQuery m_hPingServerQuery{};
    gameserveritem_t server_item_{};
    bool first_player_responded_{};

    bool m_bConnecting;
    char m_szPassword[64];

    bool m_bServerNotResponding;
    bool m_bServerFull;
    bool m_bShowAutoRetryToggle;

    bool m_bServerHadSuccessfulResponse = false;
};

#endif