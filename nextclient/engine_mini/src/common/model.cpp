#include "../engine.h"
#include <optick.h>
#include <model.h>
#include <studio.h>
#include "model.h"
#include "zone.h"
#include "sys_dll.h"
#include "../console/console.h"
#include "../l_studio.h"
#include "../graphics/gl_local.h"

char loadname[32];
model_t mod_known[MAX_KNOWN_MODELS];
int mod_numknown;
mod_known_info_t mod_known_info[MAX_KNOWN_MODELS];

qboolean gSpriteMipMap;
int gSpriteTextureFormat;
uint8_t* pspritepal;

void Mod_LoadAliasModel(model_t* mod, void* buffer)
{
    OPTICK_EVENT();

    eng()->Mod_LoadAliasModel.InvokeChained(mod, buffer);
}

void Mod_LoadBrushModel(model_t* mod, void* buffer)
{
    OPTICK_EVENT();

    eng()->Mod_LoadBrushModel.InvokeChained(mod, buffer);
}

void DT_LoadDetailMapFile(char* levelName)
{
    OPTICK_EVENT();

    eng()->DT_LoadDetailMapFile.InvokeChained(levelName);
}

void* Mod_Extradata(model_t *mod)
{
    OPTICK_EVENT();

    if (!mod)
        return nullptr;

    void *r = Cache_Check(&mod->cache);
    if (r)
        return r;

    if (mod->type == mod_brush)
        Sys_Error("%s: called with mod_brush!\n", __func__);

    Mod_LoadModel(mod, true, false);

    if (mod->cache.data == nullptr)
        Sys_Error("%s: caching failed", __func__);

    return mod->cache.data;
}

void Mod_FillInCRCInfo(qboolean trackCRC, int model_number)
{
    OPTICK_EVENT();

    mod_known_info_t* p;

    p = &mod_known_info[model_number];
    p->shouldCRC = trackCRC;
    p->firstCRCDone = 0;
    p->initialCRC = 0;
}

model_t* Mod_FindName(qboolean trackCRC, const char* name)
{
    OPTICK_EVENT();

    model_t* avail;
    int i;
    model_t* mod;

    avail = NULL;
    if (!name[0])
        Sys_Error("Mod_ForName: NULL name");

    for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
    {
        if (!Q_stricmp(mod->name, name))
            break;

        if (mod->needload == NL_UNREFERENCED)
        {
            if (!avail || mod->type != mod_alias && mod->type != mod_studio)
                avail = mod;
        }
    }

    if (i == mod_numknown)
    {
        if (mod_numknown < MAX_KNOWN_MODELS)
        {
            Mod_FillInCRCInfo(trackCRC, mod_numknown);
            ++mod_numknown;
        }
        else
        {
            if (!avail)
                Sys_Error("mod_numknown >= MAX_KNOWN_MODELS");
            mod = avail;
            Mod_FillInCRCInfo(trackCRC, avail - mod_known);
        }
        Q_strncpy(mod->name, name, MAX_MODEL_NAME);

        if (mod->needload != NL_CLIENT)
            mod->needload = NL_NEEDS_LOADED;
    }

    return mod;
}

qboolean Mod_ValidateCRC(const char* name, CRC32_t crc)
{
    OPTICK_EVENT();

    model_t* mod;
    mod_known_info_t* p;

    mod = Mod_FindName(TRUE, name);
    p = &mod_known_info[mod - mod_known];

    if (p->firstCRCDone)
    {
        if (p->initialCRC != crc)
            return FALSE;
    }
    return TRUE;
}

void Mod_NeedCRC(const char* name, qboolean needCRC)
{
    OPTICK_EVENT();

    model_t* mod;
    mod_known_info_t* p;

    mod = Mod_FindName(FALSE, name);
    p = &mod_known_info[mod - mod_known];

    p->shouldCRC = needCRC;
}

void* Mod_LoadSpriteFrame(void* pin, mspriteframe_t** ppframe, int framenum)
{
	OPTICK_EVENT();

	dspriteframe_t* pinframe = (dspriteframe_t*) pin;
	int origin[2] = {LittleLong(pinframe->origin[0]), LittleLong(pinframe->origin[1])};
	int width = LittleLong(pinframe->width);
	int height = LittleLong(pinframe->height);
	int size = width * height;

	mspriteframe_t* pspriteframe = (mspriteframe_t*) Hunk_AllocName(sizeof(mspriteframe_t), loadname);
	Q_memset(pspriteframe, 0, sizeof(mspriteframe_t));
	*ppframe = pspriteframe;
	pspriteframe->width = width;
	pspriteframe->height = height;
	pspriteframe->up = (float) origin[1];
	pspriteframe->down = (float) (origin[1] - height);
	pspriteframe->left = (float) origin[0];
	pspriteframe->right = (float) (width + origin[0]);

	//
	// load texture
	//
	int iType = 3;
	if (gSpriteTextureFormat == 0 || gSpriteTextureFormat == 1)
		iType = 0;
	else if (gSpriteTextureFormat == 3)
		iType = 1;

	char texture_id[256];
	V_snprintf(texture_id, sizeof(texture_id), "%s_%i", (*loadmodel)->name, framenum);

	uint8_t palette[256 * 3];
	Q_memcpy(palette, pspritepal, sizeof(palette));

	if (gSpriteMipMap)
		pspriteframe->gl_texturenum = GL_LoadTexture(texture_id, GLT_SPRITE_0, width, height, (uint8_t*)pin + sizeof(dspriteframe_t), gSpriteMipMap, iType, palette);
	else
		pspriteframe->gl_texturenum = GL_LoadTexture(texture_id, GLT_HUDSPRITE_0, width, height, (uint8_t*)pin + sizeof(dspriteframe_t), 0, iType, palette);

	return (uint8_t*) pinframe + sizeof(dspriteframe_t) + size;
}

void* Mod_LoadSpriteGroup(void* pin, mspriteframe_t** ppframe, int framenum)
{
    OPTICK_EVENT();

	dspritegroup_t* pingroup = (dspritegroup_t*) pin;
	int numframes = LittleLong(pingroup->numframes);

	mspritegroup_t* pspritegroup = (mspritegroup_t*) Hunk_AllocName(sizeof(mspritegroup_t) + (numframes - 1) * sizeof(pspritegroup->frames[0]), loadname);
	pspritegroup->numframes = numframes;
	*ppframe = (mspriteframe_t*) pspritegroup;

	dspriteinterval_t* pin_intervals = (dspriteinterval_t*) (pingroup + 1);

	float* poutintervals = (float*) Hunk_AllocName(numframes * sizeof(float), loadname);
	pspritegroup->intervals = poutintervals;

	for (int i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat(&pin_intervals->interval, &pin_intervals->interval);
		if (*poutintervals <= 0.0f)
			Sys_Error("%s: interval<=0", __func__);

		poutintervals++;
		pin_intervals++;
	}

	void* ptemp = pin_intervals;
	for (int i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadSpriteFrame(ptemp, &pspritegroup->frames[i], i + framenum * 100);
	}

	return ptemp;
}

void Mod_LoadSpriteModel(model_t *mod, void *buffer)
{
	OPTICK_EVENT();

	dsprite_t* pin = (dsprite_t*) buffer;

	int version = LittleLong(pin->version);
	if (version != SPRITE_VERSION)
		Sys_Error("%s: %s has wrong version number (%i should be %i)", __func__, mod->name, version, SPRITE_VERSION);

	int numframes = LittleLong(pin->numframes);
	int palcolorsnum = *(uint16_t*) &pin[1];
	int size = sizeof(msprite_t) + (numframes - 1) * sizeof(msprite_t::frames) + sizeof(uint16_t) + 8 * palcolorsnum;

	gSpriteTextureFormat = LittleLong(pin->texFormat);

	msprite_t* psprite = (msprite_t*) Hunk_AllocName(size, loadname);
	psprite->type = LittleLong(pin->type);
	psprite->maxwidth = LittleLong(pin->width);
	psprite->maxheight = LittleLong(pin->height);
	psprite->beamlength = LittleFloat(&pin->beamlength, &pin->beamlength);
	psprite->numframes = numframes;

	mod->type = mod_sprite;
	mod->numframes = numframes;
	mod->synctype = (synctype_t) LittleLong(pin->synctype);
	mod->flags = 0;
	mod->mins[0] = mod->mins[1] = float(-psprite->maxwidth / 2);
	mod->maxs[0] = mod->maxs[1] = float(psprite->maxwidth / 2);
	mod->mins[2] = float(-psprite->maxheight / 2);
	mod->maxs[2] = float(psprite->maxheight / 2);
	mod->cache.data = psprite;

	pspritepal = (unsigned char*) (&pin[1]) + sizeof(uint16_t); // header + palette size

	//
	// load the frames
	//
	if (numframes < 1)
		Sys_Error("%s: Invalid # of frames: %d\n", __func__, numframes);

	dspriteframetype_t* pframetype = (dspriteframetype_t*) ((char*) (pin + 1) + 2 + 3 * palcolorsnum);

	for (int i = 0; i < numframes; i++)
	{
		spriteframetype_t frametype = (spriteframetype_t) LittleLong(pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
			pframetype = (dspriteframetype_t*) Mod_LoadSpriteFrame(pframetype + 1, &psprite->frames[i].frameptr, i);
		else
			pframetype = (dspriteframetype_t*) Mod_LoadSpriteGroup(pframetype + 1, &psprite->frames[i].frameptr, i);
	}
}

void Mod_LoadStudioModel(model_t* mod, void* buffer)
{
    OPTICK_EVENT();

    studiohdr_t* phdr = (studiohdr_t*) buffer;
    if (LittleLong(phdr->version) != STUDIO_VERSION)
    {
        Q_memset(phdr, 0, 244u);
        Q_strcpy(phdr->name, "bogus");
        phdr->length = 244;
        phdr->texturedataindex = 244;
    }

    mod->type = mod_studio;
    mod->flags = phdr->flags;

    int length = phdr->textureindex ? phdr->texturedataindex : phdr->length;

    Cache_Alloc(&mod->cache, length, mod->name);
    uint8_t* pout = (uint8_t*) mod->cache.data;
    if (pout)
    {
        Q_memcpy(pout, buffer, length);

        if (phdr->textureindex)
        {
            mstudiotexture_t* ptexture = (mstudiotexture_t*) (pout + phdr->textureindex);

            for (int i = 0; i < phdr->numtextures; i++, ptexture++)
            {
                uint8_t* ppal = (uint8_t*)buffer + ptexture->index + ptexture->height * ptexture->width;

                char texture_id[256];
                V_snprintf(texture_id, sizeof(texture_id), "%s%s", mod->name, ptexture->name);

               ptexture->index = GL_LoadTexture(texture_id,
                   GLT_STUDIO_0,
                   ptexture->width,
                   ptexture->height,
                   (uint8_t*)buffer + ptexture->index,
                   (ptexture->flags & STUDIO_NF_NOMIPS) != 0,
                   (ptexture->flags & STUDIO_NF_MASKED) != 0,
                   ppal);
            }
        }
    }
}

model_t* Mod_LoadModel(model_t* mod, qboolean crash, qboolean trackCRC)
{
    OPTICK_EVENT();

    unsigned char* buf;
    char tmpName[MAX_PATH];
    int length;
    CRC32_t currentCRC;

    if (mod->type == mod_alias || mod->type == mod_studio)
    {
        if (Cache_Check(&mod->cache))
        {
            mod->needload = NL_PRESENT;
            return mod;
        }
    }
    else
    {
        if (mod->needload == NL_PRESENT || mod->needload == NL_CLIENT)
            return mod;
    }

    if (COM_CheckParm("-steam") && mod->name[0] == '/')
    {
        char* p = mod->name;
        while (*(++p) == '/');

        Q_strncpy(tmpName, p, sizeof(tmpName));
        Q_strncpy(mod->name, tmpName, sizeof(mod->name));
    }

    // load the file
    buf = COM_LoadFileForMe(mod->name, &length);
    if (!buf)
    {
        if (crash)
            Sys_Error("%s: %s not found", __func__, mod->name);

        return nullptr;
    }

    if (trackCRC)
    {
        mod_known_info_t* p = &mod_known_info[mod - mod_known];
        if (p->shouldCRC)
        {
            g_engfuncs.pfnCRC32_Init(&currentCRC);
            g_engfuncs.pfnCRC32_ProcessBuffer(&currentCRC, buf, length);
            currentCRC = g_engfuncs.pfnCRC32_Final(currentCRC);
            if (p->firstCRCDone)
            {
                if (currentCRC != p->initialCRC)
                {
                    p->initialCRC = currentCRC;
                    Con_DPrintf(ConLogType::Info, "%s: %s model changed on disk, reloading...\n", __func__, mod->name);
                }
            }
            else
            {
                p->firstCRCDone = 1;
                p->initialCRC = currentCRC;
            }
        }
    }

    COM_FileBase(mod->name, loadname);
    *loadmodel = mod;

    mod->needload = NL_PRESENT;

    switch (LittleLong(*(uint32*) buf))
    {
        case IDPOLYHEADER:
            // old-format of the model from the quake1
            Mod_LoadAliasModel(mod, buf);
            break;
        case IDSPRITEHEADER:
            Mod_LoadSpriteModel(mod, buf);
            break;
        case IDSTUDIOHEADER:
            Mod_LoadStudioModel(mod, buf);
            break;
        case Q1BSP_VERSION:
        case HLBSP_VERSION:
            DT_LoadDetailMapFile(loadname);
            Mod_LoadBrushModel(mod, buf);
        break;

        default:
            Mem_Free(buf);
            if (crash)
                Sys_Error("%s: %s has unknown format\n", __func__, mod->name);
            else
                Con_Printf("%s: %s has unknown format\n", __func__, mod->name);
        return nullptr;
    }

    if (g_modfuncs.m_pfnModelLoad)
        g_modfuncs.m_pfnModelLoad(mod, buf);

    Mem_Free(buf);
    return mod;
}

model_t* Mod_ForName(const char* name, qboolean crash, qboolean trackCRC)
{
    OPTICK_EVENT();

    model_t* mod;

    mod = Mod_FindName(trackCRC, name);
    if (!mod)
        return NULL;

    return Mod_LoadModel(mod, crash, trackCRC);
}

void Mod_Print()
{
    OPTICK_EVENT();

    int i;
    model_t* mod;

    Con_Printf("Cached models:\n");

    for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
    {
        Con_Printf("%8p : %s", mod->cache.data, mod->name);
        if (mod->needload & NL_UNREFERENCED)
            Con_Printf(" (!R)");
        if (mod->needload & NL_NEEDS_LOADED)
            Con_Printf(" (!P)");
        Con_Printf("\n");
    }
}

void Mod_UnloadSpriteTextures(model_t* pModel)
{
    OPTICK_EVENT();

    if (!pModel)
        return;

    if (pModel->type != mod_sprite)
        return;

    pModel->needload = NL_NEEDS_LOADED;

    msprite_t* spr = (msprite_t*)pModel->cache.data;
    if (spr)
    {
        char name[256];
        for (int i = 0; i < spr->numframes; i++)
        {
            V_snprintf(name, sizeof(name), "%s_%i", pModel->name, i);
            eng()->GL_UnloadTexture(name);
        }
    }
}

void Mod_UnloadFiltered(const std::function<bool(model_t*)>& filter)
{
    OPTICK_EVENT();

    model_t* mod;
    mod_known_info_t* p;

    for (int i = 0; i < mod_numknown; i++)
    {
        mod = &mod_known[i];

        if (!filter(mod))
            continue;

        Con_DPrintf(ConLogType::Info, "%s: unloading \"%s\"\n", __func__, mod->name);

        if (mod->type == mod_studio)
        {
            if (Cache_Check(&mod->cache))
                Cache_Free(&mod->cache);
        }
        else if (mod->type == mod_brush)
        {
            // TODO unload brush model!!
        }
        else if (mod->type == mod_sprite)
        {
            Mod_UnloadSpriteTextures(mod);
        }

        Q_memset(mod, 0, sizeof(model_t));
        mod->needload = NL_UNREFERENCED;

        p = &mod_known_info[i];
        Q_memset(p, 0, sizeof(mod_known_info_t));
    }
}

void Mod_ClearAll()
{
    OPTICK_EVENT();

    model_t* mod;

    for (int i = 0; i < mod_numknown; i++)
    {
        mod = &mod_known[i];

        if (mod->type != mod_alias && mod->needload != NL_CLIENT)
        {
            mod->needload = NL_UNREFERENCED;

            if (mod->type == mod_sprite)
                mod->cache.data = nullptr;
        }
    }
}

void Mod_ChangeGame()
{
    OPTICK_EVENT();

    int i;
    model_t* mod;
    mod_known_info_t* p;

    for (i = 0; i < mod_numknown; i++)
    {
        mod = &mod_known[i];

        if (mod->type == mod_studio)
        {
            if (Cache_Check(&mod->cache))
                Cache_Free(&mod->cache);
        }

        p = &mod_known_info[i];

        p->firstCRCDone = FALSE;
        p->initialCRC = 0;
    }
}
