#pragma once

// values for model_t's needload
enum
{
    NL_PRESENT = 0,
    NL_NEEDS_LOADED,
    NL_UNREFERENCED,
    NL_CLIENT
};

extern char loadname[32];
extern model_t mod_known[MAX_KNOWN_MODELS];
extern int mod_numknown;

extern qboolean gSpriteMipMap;

void* Mod_Extradata(model_t *mod);
model_t* Mod_FindName(qboolean trackCRC, const char* name);
qboolean Mod_ValidateCRC(const char* name, CRC32_t crc);
void Mod_NeedCRC(const char* name, qboolean needCRC);
model_t* Mod_LoadModel(model_t* mod, qboolean crash, qboolean trackCRC);
model_t* Mod_ForName(const char* name, qboolean crash, qboolean trackCRC);
void Mod_Print();
void Mod_UnloadSpriteTextures(model_t* pModel);
void Mod_UnloadFiltered(const std::function<bool(model_t*)>& filter);
void Mod_ClearAll();
void Mod_ChangeGame();

int* R_StudioReloadSkin(model_t* pModel, int index, skin_t* pskin);
