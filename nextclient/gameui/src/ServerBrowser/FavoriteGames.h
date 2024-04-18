#ifndef FAVORITEGAMES_H
#define FAVORITEGAMES_H

#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

#include "IGameList.h"
#include "serveritem.h"

class CFavoriteGames : public CBaseGamesPage
{
    DECLARE_CLASS_SIMPLE(CFavoriteGames, CBaseGamesPage);

    void AddNewServer(uint32_t ip, uint16_t port);

public:
    explicit CFavoriteGames(vgui2::Panel *parent);
    ~CFavoriteGames() override;

    void OnPageShow() override;
    void OnPageHide() override;
    void OnViewGameInfo() override;

    bool SupportsItem(InterfaceItem item) override;
    void StartRefresh() override;
    void GetNewServerList() override;
    void StopRefresh(CancelQueryReason reason) override;

    GuiConnectionSource GetConnectionSource() override;

protected:
    // IServerRefreshResponse
    void RefreshComplete() override;
    void ServerFailedToRespond(serveritem_t &server) override;

private:
    MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);
    MESSAGE_FUNC(OnRemoveFromFavorites, "RemoveFromFavorites");
    MESSAGE_FUNC(OnAddServerByName, "AddServerByName");

private:
    void OnRefreshServer(int serverID);
    void OnAddCurrentServer();
    void OnCommand(const char *command);

};

#endif