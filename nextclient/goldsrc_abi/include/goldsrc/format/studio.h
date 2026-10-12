#pragma once

#include <cstdint>

#include <goldsrc/fwd.h>
#include <goldsrc/types.h>

namespace goldsrc
{
    inline constexpr int MAXSTUDIOBONES = 128;
    inline constexpr int MAXSTUDIOSKINS = 100;
    inline constexpr int STUDIO_HAS_NORMALS = 0x00000001;
    inline constexpr int STUDIO_LOOPING = 0x00000001;
    inline constexpr int STUDIO_NF_FLATSHADE = 0x00000001;
    inline constexpr int STUDIO_X = 0x00000001;
    inline constexpr int STUDIO_HAS_VERTICES = 0x00000002;
    inline constexpr int STUDIO_NF_CHROME = 0x00000002;
    inline constexpr int STUDIO_Y = 0x00000002;
    inline constexpr int STUDIO_HAS_BBOX = 0x00000004;
    inline constexpr int STUDIO_NF_FULLBRIGHT = 0x00000004;
    inline constexpr int STUDIO_Z = 0x00000004;
    inline constexpr int STUDIO_HAS_CHROME = 0x00000008;
    inline constexpr int STUDIO_NF_NOMIPS = 0x00000008;
    inline constexpr int STUDIO_XR = 0x00000008;
    inline constexpr int STUDIO_NF_ALPHA = 0x00000010;
    inline constexpr int STUDIO_YR = 0x00000010;
    inline constexpr int STUDIO_NF_ADDITIVE = 0x00000020;
    inline constexpr int STUDIO_ZR = 0x00000020;
    inline constexpr int STUDIO_LX = 0x00000040;
    inline constexpr int STUDIO_NF_MASKED = 0x00000040;
    inline constexpr int STUDIO_LY = 0x00000080;
    inline constexpr int STUDIO_DYNAMIC_LIGHT = 0x00000100;
    inline constexpr int STUDIO_LZ = 0x00000100;
    inline constexpr int STUDIO_AX = 0x00000200;
    inline constexpr int STUDIO_TRACE_HITBOX = 0x00000200;
    inline constexpr int STUDIO_AY = 0x00000400;
    inline constexpr int STUDIO_AZ = 0x00000800;
    inline constexpr int STUDIO_AXR = 0x00001000;
    inline constexpr int STUDIO_AYR = 0x00002000;
    inline constexpr int STUDIO_AZR = 0x00004000;
    inline constexpr int STUDIO_TYPES = 0x00007FFF;
    inline constexpr int STUDIO_RLOOP = 0x00008000;
    inline constexpr int STUDIO_CACHE_SIZE = 16;
    inline constexpr int STUDIO_MOUTH = 4;
    inline constexpr int STUDIO_NUM_HULLS = 128;
    inline constexpr int STUDIO_NUM_PLANES = 768;
    inline constexpr double STUDIO_TO_RAD = 9.587379924285257e-05;

    struct mstudioanim_t
    {
        unsigned short offset[6];
    };

    union mstudioanimvalue_t
    {
        struct
        {
            uint8_t valid;
            uint8_t total;
        } num;
        short value;
    };

    struct mstudioattachment_t
    {
        char name[32];
        int type;
        int bone;
        vec3_t org;
        vec3_t vectors[3];
    };

    struct mstudiobodyparts_t
    {
        char name[64];
        int nummodels;
        int base;
        int modelindex;
    };

    struct mstudiobone_t
    {
        char name[32];
        int parent;
        int flags;
        int bonecontroller[6];
        float value[6];
        float scale[6];
    };

    struct mstudioevent_s
    {
        int frame;
        int event;
        int type;
        char options[64];
    };

    struct mstudioseqdesc_t
    {
        char label[32];
        float fps;
        int flags;
        int activity;
        int actweight;
        int numevents;
        int eventindex;
        int numframes;
        int numpivots;
        int pivotindex;
        int motiontype;
        int motionbone;
        vec3_t linearmovement;
        int automoveposindex;
        int automoveangleindex;
        vec3_t bbmin;
        vec3_t bbmax;
        int numblends;
        int animindex;
        int blendtype[2];
        float blendstart[2];
        float blendend[2];
        int blendparent;
        int seqgroup;
        int entrynode;
        int exitnode;
        int nodeflags;
        int nextseq;
    };

    struct mstudiotexture_t
    {
        char name[64];
        int flags;
        int width;
        int height;
        int index;
    };

    struct mstudiobonecontroller_t
    {
        int bone;
        int type;
        float start;
        float end;
        int rest;
        int index;
    };

    struct mstudiomesh_t
    {
        int numtris;
        int triindex;
        int skinref;
        int numnorms;
        int normindex;
    };

    struct mstudiomodel_t
    {
        char name[64];
        int type;
        float boundingradius;
        int nummesh;
        int meshindex;
        int numverts;
        int vertinfoindex;
        int vertindex;
        int numnorms;
        int norminfoindex;
        int normindex;
        int numgroups;
        int groupindex;
    };

    struct studiohdr_t
    {
        int id;
        int version;
        char name[64];
        int length;
        vec3_t eyeposition;
        vec3_t min;
        vec3_t max;
        vec3_t bbmin;
        vec3_t bbmax;
        int flags;
        int numbones;
        int boneindex;
        int numbonecontrollers;
        int bonecontrollerindex;
        int numhitboxes;
        int hitboxindex;
        int numseq;
        int seqindex;
        int numseqgroups;
        int seqgroupindex;
        int numtextures;
        int textureindex;
        int texturedataindex;
        int numskinref;
        int numskinfamilies;
        int skinindex;
        int numbodyparts;
        int bodypartindex;
        int numattachments;
        int attachmentindex;
        int soundtable;
        int soundindex;
        int soundgroups;
        int soundgroupindex;
        int numtransitions;
        int transitionindex;
    };
} // namespace goldsrc
