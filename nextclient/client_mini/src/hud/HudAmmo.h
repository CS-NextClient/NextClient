#pragma once

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudAmmo : public HudBase, public HudBaseHelper
{
public:
    explicit HudAmmo(nitroapi::NitroApiInterface* nitro_api);
    ~HudAmmo() override;

private:
    static void AmmoXHook(const char* name, int size, void* data);
};
