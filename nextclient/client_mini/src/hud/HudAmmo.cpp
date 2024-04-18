#include "HudAmmo.h"
#include <parsemsg.h>

HudAmmo::HudAmmo(nitroapi::NitroApiInterface* nitro_api) :
    HudBaseHelper(nitro_api)
{
    DeferUnsub(cl()->UserMsg_AmmoX |= [](const char* name, int size, void* data, const auto& next) {
        AmmoXHook(name, size, data);
        return next->Invoke(name, size, data);
    });
}

HudAmmo::~HudAmmo()
{
}

void HudAmmo::AmmoXHook(const char* name, int size, void* data)
{
    BEGIN_READ(data, size);
    int ammoid = READ_BYTE();
    int amount = READ_BYTE();

    // fix invalid ammoid value, the original client.dll accesses the gWR.rgWeapons by this index without checking
    if (ammoid < 0 || ammoid > 31)
    {
        BufferWriter buffer_writer;
        buffer_writer.Init(static_cast<unsigned char*>(data), size);
        buffer_writer.WriteByte(0);
        buffer_writer.WriteByte(amount);
    }
}
