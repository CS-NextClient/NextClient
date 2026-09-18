#pragma once

#include <cstdint>

#include <goldsrc/format/sprite.h>
#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    using PFNCACHE = void (*)(cachewad_t*, unsigned char*);

    inline constexpr int IDPOLYHEADER = 1330660425;
    inline constexpr int MAX_KNOWN_MODELS = 1024;
    inline constexpr int SURF_PLANEBACK = 0x00000002;
    inline constexpr int SURF_DRAWSKY = 0x00000004;
    inline constexpr int SURF_DRAWSPRITE = 0x00000008;
    inline constexpr int SURF_DRAWTURB = 0x00000010;
    inline constexpr int SURF_DRAWTILED = 0x00000020;
    inline constexpr int SURF_DRAWBACKGROUND = 0x00000040;
    inline constexpr int SURF_UNDERWATER = 0x00000080;
    inline constexpr int MAX_MODEL_NAME = 64;

    enum modtype_t
    {
        mod_bad = -1,
        mod_brush = 0,
        mod_sprite = 1,
        mod_alias = 2,
        mod_studio = 3
    };

    struct cacheentry_t
    {
        cacheentry_t() = delete;
        cacheentry_t(const cacheentry_t&) = delete;
        cacheentry_t& operator=(const cacheentry_t&) = delete;

        char name[64];
        cache_user_t cache;
    };

    struct glpoly_t
    {
        glpoly_t() = delete;
        glpoly_t(const glpoly_t&) = delete;
        glpoly_t& operator=(const glpoly_t&) = delete;

        glpoly_t* next;
        glpoly_t* chain;
        int numverts;
        int flags;
        float verts[4][7];
    };

    struct hull_t
    {
        dclipnode_t* clipnodes;
        mplane_t* planes;
        int firstclipnode;
        int lastclipnode;
        vec3_t clip_mins;
        vec3_t clip_maxs;
    };

    struct medge_t
    {
        medge_t() = delete;
        medge_t(const medge_t&) = delete;
        medge_t& operator=(const medge_t&) = delete;

        unsigned short v[2];
    };

    struct mod_known_info_t
    {
        mod_known_info_t() = delete;
        mod_known_info_t(const mod_known_info_t&) = delete;
        mod_known_info_t& operator=(const mod_known_info_t&) = delete;

        qboolean shouldCRC;
        qboolean firstCRCDone;
        CRC32_t initialCRC;
    };

    struct mplane_t
    {
        mplane_t() = delete;
        mplane_t(const mplane_t&) = delete;
        mplane_t& operator=(const mplane_t&) = delete;

        vec3_t normal;
    };

    struct mspriteframe_t
    {
        mspriteframe_t() = delete;
        mspriteframe_t(const mspriteframe_t&) = delete;
        mspriteframe_t& operator=(const mspriteframe_t&) = delete;

        int width;
        int height;
        float up;
        float down;
        float left;
        float right;
        int gl_texturenum;
    };

    struct mspriteframedesc_t
    {
        spriteframetype_t type;
        mspriteframe_t* frameptr;
    };

    struct mspritegroup_t
    {
        mspritegroup_t() = delete;
        mspritegroup_t(const mspritegroup_t&) = delete;
        mspritegroup_t& operator=(const mspritegroup_t&) = delete;

        int numframes;
        float* intervals;
        mspriteframe_t* frames[1];
    };

    struct msurface_t
    {
        msurface_t() = delete;
        msurface_t(const msurface_t&) = delete;
        msurface_t& operator=(const msurface_t&) = delete;

        int visframe;
        mplane_t* plane;
        int flags;
        int firstedge;
        int numedges;
        short texturemins[2];
        short extents[2];
        int light_s;
        int light_t;
        glpoly_t* polys;
        msurface_t* texturechain;
        mtexinfo_t* texinfo;
        int dlightframe;
        int dlightbits;
        int lightmaptexturenum;
    };

    struct mtexinfo_t
    {
        mtexinfo_t() = delete;
        mtexinfo_t(const mtexinfo_t&) = delete;
        mtexinfo_t& operator=(const mtexinfo_t&) = delete;

        float vecs[2][4];
        float mipadjust;
        texture_t* texture;
    };

    struct mvertex_t
    {
        mvertex_t() = delete;
        mvertex_t(const mvertex_t&) = delete;
        mvertex_t& operator=(const mvertex_t&) = delete;

        vec3_t position;
    };

    struct skin_t
    {
        skin_t() = delete;
        skin_t(const skin_t&) = delete;
        skin_t& operator=(const skin_t&) = delete;

        int keynum;
        int topcolor;
        int bottomcolor;
        int model;
        char name[260];
        int index;
        int source;
        int width;
        int height;
    };

    struct texture_t
    {
        texture_t() = delete;
        texture_t(const texture_t&) = delete;
        texture_t& operator=(const texture_t&) = delete;

        char name[16];
        unsigned int width;
        unsigned int height;
        int gl_texturenum;
    };

    struct cachewad_t
    {
        cachewad_t() = delete;
        cachewad_t(const cachewad_t&) = delete;
        cachewad_t& operator=(const cachewad_t&) = delete;

        char* name;
        cacheentry_t* cache;
        int cacheCount;
        int cacheMax;
        lumpinfo_t* lumps;
        int lumpCount;
        int cacheExtra;
        PFNCACHE pfnCacheBuild;
        int numpaths;
        char** basedirs;
        int* lumppathindices;
        int tempWad;
    };

    struct model_t
    {
        model_t() = delete;
        model_t(const model_t&) = delete;
        model_t& operator=(const model_t&) = delete;

        char name[64];
        int needload;
        modtype_t type;
        int numframes;
        synctype_t synctype;
        int flags;
        vec3_t mins;
        vec3_t maxs;
        float radius;
        int firstmodelsurface;
        int nummodelsurfaces;
        int numsubmodels;
        dmodel_t* submodels;
        int numplanes;
        mplane_t* planes;
        int numleafs;
        mleaf_s* leafs;
        int numvertexes;
        mvertex_t* vertexes;
        int numedges;
        medge_t* edges;
        int numnodes;
        mnode_t* nodes;
        int numtexinfo;
        mtexinfo_t* texinfo;
        int numsurfaces;
        msurface_t* surfaces;
        int numsurfedges;
        int* surfedges;
        int numclipnodes;
        dclipnode_t* clipnodes;
        int nummarksurfaces;
        msurface_t** marksurfaces;
        hull_t hulls[4];
        int numtextures;
        texture_t** textures;
        uint8_t* visdata;
        color24* lightdata;
        char* entities;
        cache_user_t cache;
    };

    struct msprite_t
    {
        msprite_t() = delete;
        msprite_t(const msprite_t&) = delete;
        msprite_t& operator=(const msprite_t&) = delete;

        short type;
        short texFormat;
        int maxwidth;
        int maxheight;
        int numframes;
        int paloffset;
        float beamlength;
        void* cachespot;
        mspriteframedesc_t frames[1];
    };
} // namespace goldsrc
