#include "HudAmmo.h"
#include <data_types/MessageReader.h>
#include <data_types/MessageWriter.h>

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
    MessageReader reader(data, size);
    int ammoid = reader.ReadByte();
    int amount = reader.ReadByte();

    // fix invalid ammoid value, the original client.dll accesses the gWR.rgWeapons by this index without checking
    if (ammoid < 0 || ammoid > 31)
    {
        MessageWriter writer(data, size);
        writer.WriteByte(0);
        writer.WriteByte(amount);
    }
}
