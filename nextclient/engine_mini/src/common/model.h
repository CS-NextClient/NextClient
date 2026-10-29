#pragma once

#include <goldsrc/engine/model.h>
#include <goldsrc/types.h>

// values for goldsrc::model_t's needload
enum
{
    NL_PRESENT = 0,
    NL_NEEDS_LOADED,
    NL_UNREFERENCED,
    NL_CLIENT
};

extern char loadname[32];
extern goldsrc::model_t mod_known[goldsrc::MAX_KNOWN_MODELS];
extern int mod_numknown;

extern goldsrc::qboolean gSpriteMipMap;

void* Mod_Extradata(goldsrc::model_t* mod);
goldsrc::model_t* Mod_FindName(goldsrc::qboolean trackCRC, const char* name);
goldsrc::qboolean Mod_ValidateCRC(const char* name, goldsrc::CRC32_t crc);
void Mod_NeedCRC(const char* name, goldsrc::qboolean needCRC);
void Mod_LoadStudioModel(goldsrc::model_t* mod, void* buffer, int file_length);
goldsrc::model_t* Mod_LoadModel(goldsrc::model_t* mod, goldsrc::qboolean crash, goldsrc::qboolean trackCRC);
goldsrc::model_t* Mod_ForName(const char* name, goldsrc::qboolean crash, goldsrc::qboolean trackCRC);
void Mod_Print();
void Mod_UnloadSpriteTextures(goldsrc::model_t* pModel);
void Mod_UnloadFiltered(const std::function<bool(goldsrc::model_t*)>& filter);
void Mod_ClearAll();
void Mod_ResetState();

int* R_StudioReloadSkin(goldsrc::model_t* pModel, int index, goldsrc::skin_t* pskin);
