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
        char name[64];
        cache_user_t cache;
    };

    struct glpoly_t
    {
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

    struct alignas(4) medge_t
    {
        unsigned short v[2];
        unsigned char unmodelled_[8 - (0 + sizeof(v))];
    };

    struct mod_known_info_t
    {
        qboolean shouldCRC;
        qboolean firstCRCDone;
        CRC32_t initialCRC;
    };

    struct alignas(4) mplane_t
    {
        vec3_t normal;
        float dist;
        unsigned char unmodelled_[20 - (12 + sizeof(dist))];
    };

    struct mspriteframe_t
    {
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
        int numframes;
        float* intervals;
        mspriteframe_t* frames[1];
    };

    struct alignas(4) msurface_t
    {
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
        unsigned char unmodelled_[92 - (56 + sizeof(lightmaptexturenum))];
    };

    struct alignas(4) mtexinfo_t
    {
        float vecs[2][4];
        float mipadjust;
        texture_t* texture;
        unsigned char unmodelled_[44 - (36 + sizeof(texture))];
    };

    struct mvertex_t
    {
        vec3_t position;
    };

    struct alignas(4) skin_t
    {
        int keynum;
        int topcolor;
        int bottomcolor;
        int model;
        char name[260];
        int index;
        int source;
        int width;
        int height;
        unsigned char unmodelled_[296 - (288 + sizeof(height))];
    };

    struct alignas(4) texture_t
    {
        char name[16];
        unsigned int width;
        unsigned int height;
        int gl_texturenum;
        unsigned char unmodelled_[72 - (24 + sizeof(gl_texturenum))];
    };

    struct cachewad_t
    {
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
