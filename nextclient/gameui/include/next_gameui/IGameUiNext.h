#pragma once

#include "tier1/interface.h"
#include "steam/steam_api.h"
#include "vmodes.h"
#include "quakedef.h"
#include "APIProxy.h"
#include "IGameUI.h"

enum class GuiConnectionSource
{
    Unknown          = 0,
    ServersInternet  = 1,
    ServersFavorites = 2,
    ServersHistory   = 3,
    ServersSpectate  = 4,
    GameMenu         = 5,
    ServersUnique    = 6,
};

enum class SecureFilter
{
    All = 0,
    Secure = 1,
    UnSecure = 2
};

struct FilterState
{
    bool no_empty_servers;
    bool no_full_servers;
    int ping_filter;
    bool no_passworded_servers;
    SecureFilter secure_filter;
    char map_filter[32];
    int sort_column;
};

struct LastConnectionInfo
{
    GuiConnectionSource connection_source;
    servernetadr_t address;
    // map obtained via source query, may be empty if the connection was made from the console
    char sq_map[32];
};

class INextUICallbacks
{
public:
    virtual ~INextUICallbacks() = default;

    // Called when a player has selected a server in the server browser in the Internet tab.
    virtual void InternetServerSelected(uint32_t ip, uint16_t port, int num, int total_servers) = 0;

    // Called when the player has called a command in the main menu. The function is only called if the command was not processed by the GameUI itself.
    // Return true to indicate that the command has been processed (the following listeners will not be called).
    virtual bool RunMenuCommand(const char* command) = 0;

    // Called when the player has invoked the ConnectToRandomServer command in the main menu.
    // Return true to indicate that the address has been written (the following listeners will not be called).
    virtual bool DequeueGameMenuServer(servernetadr_t* address) = 0;
};

class IGameUINext : public IBaseInterface
{
public:
    virtual void GetVersion(int* major, int* minor, int* patch, char* buffer, int size) = 0;
    virtual void GetLastConnectionInfo(LastConnectionInfo* out) = 0;
    virtual void GetInternetFilterState(FilterState* out) = 0;
    virtual void AddCallbacksListener(INextUICallbacks* listener) = 0;
    virtual void RemoveCallbacksListener(INextUICallbacks* listener) = 0;
    // Displays a simple modal window with a single button that closes the window
    virtual void ShowMessageBox(const char* title, const char* text, const char* button_text) = 0;
};

#define GAMEUI_NEXT_INTERFACE_VERSION "GameUINext004"
