#ifndef ISERVERBROWSER_H
#define ISERVERBROWSER_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

class KeyValues;

namespace vgui2
{
    class Panel;
}

enum class ServerBrowserTab
{
    Internet  = 0,
    Favorites = 1,
    Unique    = 2,
    History   = 3,
    LAN       = 4,
    Friends   = 5
};

class IServerBrowserEx : public IBaseInterface
{
public:
    virtual bool Initialize(CreateInterfaceFn *factorylist, int numFactories) = 0;
    virtual void Shutdown() = 0;
    virtual void SetParent(vgui2::VPANEL parent) = 0;
    virtual void Deactivate() = 0;
    virtual void Reactivate() = 0;
    virtual bool Activate() = 0;
    virtual bool Activate(ServerBrowserTab tab) = 0;
    virtual void ActiveGameName(const char *szGameName, const char *szGameDir) = 0;
    virtual void ConnectToGame(int ip, int connectionport) = 0;
    virtual void DisconnectFromGame() = 0;
    virtual bool JoinGame(unsigned int gameIP, unsigned int gamePort, const char *userName) = 0;
    virtual void CloseAllGameInfoDialogs() = 0;
};

#define SERVERBROWSEREX_INTERFACE_VERSION "ServerBrowserEx001"

#endif