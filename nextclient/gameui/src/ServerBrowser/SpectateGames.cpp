#include "SpectateGames.h"
#include "serveritem.h"

CSpectateGames::CSpectateGames(vgui2::Panel *parent) : CInternetGames(parent, "SpectateGames")
{
}

//void CSpectateGames::RequestServers()
//{
//    char filter[2048];
//
//    strcpy(filter, filterString);
//    strcat(filter, "\\proxy\\1");
//
//    GetMasterFilter();
//
//    BaseClass::RequestServers();
//}

GuiConnectionSource CSpectateGames::GetConnectionSource()
{
    return GuiConnectionSource::ServersSpectate;
}

bool CSpectateGames::CheckPrimaryFilters(serveritem_t &server)
{
//    if (!server.proxy)
//        return false;

    return BaseClass::CheckPrimaryFilters(server);
}