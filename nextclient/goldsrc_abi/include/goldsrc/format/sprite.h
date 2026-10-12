#pragma once

#include <goldsrc/fwd.h>

namespace goldsrc
{
    inline constexpr int IDSPRITEHEADER = 1347634249;
    inline constexpr int SPRITE_VERSION = 2;

    enum spriteframetype_t
    {
        SPR_SINGLE = 0,
        SPR_GROUP = 1,
        SPR_ANGLED = 2
    };

    enum synctype_t
    {
        ST_SYNC = 0,
        ST_RAND = 1
    };

    struct dspriteframe_t
    {
        int origin[2];
        int width;
        int height;
    };

    struct dspritegroup_t
    {
        int numframes;
    };

    struct dspriteinterval_t
    {
        float interval;
    };

    struct dsprite_t
    {
        int ident;
        int version;
        int type;
        int texFormat;
        float boundingradius;
        int width;
        int height;
        int numframes;
        float beamlength;
        synctype_t synctype;
    };

    struct dspriteframetype_t
    {
        spriteframetype_t type;
    };
} // namespace goldsrc
