#pragma once

#include <nitroapi/NitroApiInterface.h>

class ClientMiniInterface : public IBaseInterface
{
public:
    virtual void Init(nitroapi::NitroApiInterface* nitro_api) = 0;
    virtual void Uninitialize() = 0;

    // since ClientMini002
    virtual void GetVersion(char* buffer, int size) = 0;
};

#define CLIENT_MINI_INTERFACE_VERSION "ClientMini002"
