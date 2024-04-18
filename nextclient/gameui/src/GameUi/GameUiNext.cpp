#include "GameUINext.h"
#include <vgui_controls/MessageBox.h>
#include <library_config.h>
#include "../ServerBrowser/ServerBrowserDialog.h"

static CGameUINext g_GameUINext;

CGameUINext &GameUINext()
{
    return g_GameUINext;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameUI, IGameUINext, GAMEUI_NEXT_INTERFACE_VERSION, g_GameUINext);

void CGameUINext::GetLastConnectionInfo(LastConnectionInfo* out)
{
    out->connection_source = last_connection_info_.connection_source;
    out->address = last_connection_info_.address;
    V_strcpy_safe(out->sq_map, last_connection_info_.sq_map);
}

void CGameUINext::GetVersion(int* major, int* minor, int* patch, char* buffer, int size)
{
    if (major != nullptr)
        *major = GAME_UI_VERSION_MAJOR;

    if (minor != nullptr)
        *minor = GAME_UI_VERSION_MINOR;

    if (patch != nullptr)
        *patch = GAME_UI_VERSION_PATCH;

    if (buffer != nullptr)
        strcpy_s(buffer, size, GAME_UI_VERSION);
}

void CGameUINext::GetInternetFilterState(FilterState *out)
{
    ServerBrowserDialog().GetInternetFilterState(out);
}

void CGameUINext::SetLastConnectionInfo(servernetadr_t address, GuiConnectionSource connection_source, const char* sq_map)
{
    last_connection_info_.address = address;
    last_connection_info_.connection_source = connection_source;
    V_strcpy_safe(last_connection_info_.sq_map, sq_map);
}

void CGameUINext::AddCallbacksListener(INextUICallbacks *listener)
{
    auto it = std::find(listeners_.cbegin(), listeners_.cend(), listener);
    if (it != listeners_.cend())
        return;

    listeners_.emplace_back(listener);
}

void CGameUINext::RemoveCallbacksListener(INextUICallbacks *listener)
{
    auto it = std::find(listeners_.cbegin(), listeners_.cend(), listener);
    if (it == listeners_.cend())
        return;

    listeners_.erase(it);
}

void CGameUINext::ShowMessageBox(const char* title, const char* text, const char* button_text)
{
    auto* dlg = new vgui2::MessageBox(title, text);

    if (button_text != nullptr)
        dlg->SetOKButtonText(button_text);

    dlg->DoModal();
}

void CGameUINext::InvokeInternetServerSelected(uint32_t ip, uint16_t port, int num, int total_servers)
{
    for (auto& listener : listeners_)
        listener->InternetServerSelected(ip, port, num, total_servers);
}

bool CGameUINext::InvokeRunMenuCommand(const char* command)
{
    if (command == nullptr)
        return false;

    for (auto& listener : listeners_)
    {
        bool handled = listener->RunMenuCommand(command);
        if (handled)
            return true;
    }

    return false;
}

bool CGameUINext::InvokeDequeueGameMenuServer(servernetadr_t* address)
{
    if (address == nullptr)
        return false;

    for (auto& listener : listeners_)
    {
        bool handled = listener->DequeueGameMenuServer(address);
        if (handled)
            return true;
    }

    *address = servernetadr_t{};
    return false;
}
