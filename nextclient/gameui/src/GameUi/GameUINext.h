#pragma once

#include <vector>
#include <next_gameui/IGameUiNext.h>

class CGameUINext : public IGameUINext
{
    LastConnectionInfo last_connection_info_{};
    std::vector<INextUICallbacks*> listeners_;

public:
    void GetLastConnectionInfo(LastConnectionInfo* out) override;
    void GetVersion(int* major, int* minor, int* patch, char* buffer, int size) override;
    void GetInternetFilterState(FilterState* out) override;
    void AddCallbacksListener(INextUICallbacks* listener) override;
    void RemoveCallbacksListener(INextUICallbacks* listener) override;
    void ShowMessageBox(const char* title, const char* text, const char* button_text) override;

    void SetLastConnectionInfo(servernetadr_t address, GuiConnectionSource connection_source, const char* sq_map);
    void InvokeInternetServerSelected(uint32_t ip, uint16_t port, int num, int total_servers);
    bool InvokeRunMenuCommand(const char* command);
    bool InvokeDequeueGameMenuServer(servernetadr_t* address);
};

extern CGameUINext &GameUINext();