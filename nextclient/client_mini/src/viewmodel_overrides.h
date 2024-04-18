#pragma once

#include <const.h>

struct viewmodel_overrides_t
{
    int rendermode;
    int renderamt;
    color24 color;
    int renderfx;
    short skin;
    int body;

    bool rendermode_override;
    bool renderamt_override;
    bool color_override;
    bool renderfx_override;
    bool skin_override;
    bool body_override;
};