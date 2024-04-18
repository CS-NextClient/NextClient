#include "../engine.h"
#include <optick.h>
#include "../common/model.h"
#include "../common/zone.h"

cache_user_t model_texture_cache[MAX_KNOWN_MODELS][32];

int* R_StudioReloadSkin(model_t* pModel, int index, skin_t* pskin)
{
    OPTICK_EVENT();

    int v3; // edx
    int* v5; // ebx
    int v6; // eax
    int* v7; // esi
    int v8; // edi
    int* v9; // ebx
    void* pModela; // [esp+Ch] [ebp+8h]

    v3 = pModel - mod_known;
    if (v3 < 0 || v3 >= MAX_KNOWN_MODELS)
        return nullptr;

    v5 = (int*) ((char*) &model_texture_cache + 4 * (index + 32 * v3));
    if (Cache_Check((cache_user_t*) v5) != nullptr)
    {
        v9 = (int*) *v5;
    }
    else
    {
        v6 = reinterpret_cast<int>(COM_LoadFileForMe(pModel->name, nullptr));
        v7 = (int*) (v6 + 80 * index + *(int*) (v6 + 184));
        pModela = (void*) v6;
        v8 = v7[17] * v7[18] + 776;
        Cache_Alloc(reinterpret_cast<cache_user_t*>(v5), v8, (char*) (pskin + 16));
        v9 = (int*) *v5;
        *v9 = v7[17];
        v9[1] = v7[18];
        Q_memcpy((void*) (v9 + 2), (void*) ((unsigned int) pModela + v7[19]), v8 - 8);
        Mem_Free(pModela);
    }

    pskin->index = index;
    pskin->width = *v9;
    pskin->height = v9[1];

    return v9 + 2;
}