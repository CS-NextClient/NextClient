#pragma once

#include <goldsrc/types.h>

namespace goldsrc
{
    inline constexpr int FCUST_FROMHPAK = 0x00000001;
    inline constexpr int FCUST_WIPEDATA = 0x00000002;
    inline constexpr int FCUST_IGNOREINIT = 0x00000004;
    inline constexpr int MAX_RESOURCE_LIST = 1280;
    inline constexpr int RES_FATALIFMISSING = 0x00000001;
    inline constexpr int RES_WASMISSING = 0x00000002;
    inline constexpr int RES_CUSTOM = 0x00000004;
    inline constexpr int RES_REQUESTED = 0x00000008;
    inline constexpr int RES_PRECACHED = 0x00000010;
    inline constexpr int RES_ALWAYS = 0x00000020;
    inline constexpr int RES_UNK_6 = 0x00000040;
    inline constexpr int RES_CHECKFILE = 0x00000080;

    enum resourcetype_t
    {
        t_sound = 0,
        t_skin = 1,
        t_model = 2,
        t_decal = 3,
        t_generic = 4,
        t_eventscript = 5,
        t_world = 6,
        rt_unk = 7,
        rt_max = 8
    };

    struct _resourceinfo_t
    {
        int size;
    };

    struct resource_t
    {
        char szFileName[64];
        resourcetype_t type;
        int nIndex;
        int nDownloadSize;
        unsigned char ucFlags;
        unsigned char rgucMD5_hash[16];
        unsigned char playernum;
        unsigned char rguc_reserved[32];
        resource_t* pNext;
        resource_t* pPrev;
    };

    struct resourceinfo_t
    {
        _resourceinfo_t info[8];
    };

    struct customization_t
    {
        qboolean bInUse;
        resource_t resource;
        qboolean bTranslated;
        int nUserData1;
        int nUserData2;
        void* pInfo;
        void* pBuffer;
        customization_t* pNext;
    };
} // namespace goldsrc
