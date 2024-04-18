#ifndef SPECTATEGAMES_H
#define SPECTATEGAMES_H

#ifdef _WIN32
#pragma once
#endif

#include "InternetGames.h"

class CSpectateGames : public CInternetGames
{
public:
    explicit CSpectateGames(vgui2::Panel *parent);

    GuiConnectionSource GetConnectionSource() override;

protected:
    //void RequestServers();
    bool CheckPrimaryFilters(serveritem_t &server) override;

private:
    typedef CInternetGames BaseClass;
};

#endif