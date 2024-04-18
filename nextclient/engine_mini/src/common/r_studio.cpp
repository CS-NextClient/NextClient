#include "../engine.h"
#include <optick.h>
#include "r_studio.h"
#include <studio.h>
#include "model.h"

int R_StudioBodyVariations(model_t *model)
{
    OPTICK_EVENT();

    if (model->type != mod_studio)
        return 0;

    studiohdr_t *shdr = (studiohdr_t *)Mod_Extradata(model);
    if (!shdr)
        return 0;

    int count = 1;
    mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((char *)shdr + shdr->bodypartindex);
    for (int i = 0; i < shdr->numbodyparts; i++, pbodypart++)
    {
        count *= pbodypart->nummodels;
    }

    return count;
}

int ModelFrameCount(model_t *model)
{
    OPTICK_EVENT();

    int count;

    if (!model)
        return 1;

    switch (model->type)
    {
        case mod_sprite:
            count = ((msprite_t*)model->cache.data)->numframes;
        break;

        case mod_studio:
            count = R_StudioBodyVariations(model);
        break;

        default:
            return 1;
    }

    if (count < 1)
        return 1;

    return count;
}
