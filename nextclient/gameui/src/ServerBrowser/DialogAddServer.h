#ifndef DIALOGADDSERVER_H
#define DIALOGADDSERVER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>
#include "ServerList.h"
#include "netadr.h"

class CBaseGamesPage;
class IGameList;

class CAddServerGameList : public vgui2::ListPanel
{
public:
    DECLARE_CLASS_SIMPLE(CAddServerGameList, vgui2::ListPanel);

public:
    CAddServerGameList(vgui2::Panel *parent, const char *panelName);

public:
    virtual void OnKeyCodeTyped(vgui2::KeyCode code);
};

class CDialogAddServer : public vgui2::Frame, public ISteamMatchmakingPingResponse
{
public:
    DECLARE_CLASS_SIMPLE(CDialogAddServer, vgui2::Frame);

public:
    friend class CAddServerGameList;

public:
   explicit CDialogAddServer(vgui2::Panel *parent);
    ~CDialogAddServer() override;

public:
    void TestServers();

    //ISteamMatchmakingPingResponse
    void ServerResponded(gameserveritem_t &server) override;
    void ServerFailedToRespond() override;

public:
    void ApplySchemeSettings(vgui2::IScheme *pScheme);

public:
    MESSAGE_FUNC(OnItemSelected, "ItemSelected");

private:
    void OnCommand(const char *command);
    void OnOK();
    void OnTick();
    void OnKeyCodeTyped(vgui2::KeyCode code);

    MESSAGE_FUNC(OnTextChanged, "TextChanged");

private:
    vgui2::Button *m_pTestServersButton;
    vgui2::Button *m_pAddServerButton;
    vgui2::Button *m_pAddSelectedServerButton;

    vgui2::PropertySheet *m_pTabPanel;
    vgui2::TextEntry *m_pTextEntry;
    CAddServerGameList *m_pDiscoveredGames;
    int m_OriginalHeight;
    CUtlVector<gameserveritem_t> m_DiscoveredServers;
    std::vector<HServerQuery> m_ServerQueries;
};

#endif