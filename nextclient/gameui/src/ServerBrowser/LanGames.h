#ifndef LANGAMES_H
#define LANGAMES_H

#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

#include "IGameList.h"
#include "serveritem.h"
#include "utlvector.h"

class CLanGames : public CBaseGamesPage
{
    DECLARE_CLASS_SIMPLE(CLanGames, CBaseGamesPage);

public:
    CLanGames(vgui2::Panel *parent, bool bAutoRefresh = true, const char *pCustomResFilename = NULL);
    ~CLanGames(void);

public:
    void OnPageShow() override;
    void OnPageHide() override;
    void OnViewGameInfo() override;

public:
    bool SupportsItem(InterfaceItem item) override;
    void StartRefresh() override;

    void ServerFailedToRespond(serveritem_t &server) override;
    void RefreshComplete() override;

    GuiConnectionSource GetConnectionSource() override;

private:
    MESSAGE_FUNC(GetNewServerList, "GetNewServerList");
    MESSAGE_FUNC_INT(OnRefreshServer, "RefreshServer", serverID);

    MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);

private:
    bool auto_refresh_;
};

#endif