#include "../engine.h"
#include <optick.h>
#include "r_studio.h"
#include <goldsrc/engine/model.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/types.h>
#include "model.h"

int R_StudioBodyVariations(goldsrc::model_t* model)
{
    OPTICK_EVENT();

    if (model->type != goldsrc::mod_studio)
        return 0;

    goldsrc::studiohdr_t* shdr = (goldsrc::studiohdr_t*)Mod_Extradata(model);
    if (!shdr)
        return 0;

    int count = 1;
    goldsrc::mstudiobodyparts_t* pbodypart = (goldsrc::mstudiobodyparts_t*)((char*)shdr + shdr->bodypartindex);
    for (int i = 0; i < shdr->numbodyparts; i++, pbodypart++)
    {
        count *= pbodypart->nummodels;
    }

    return count;
}

int ModelFrameCount(goldsrc::model_t* model)
{
    OPTICK_EVENT();

    int count;

    if (!model)
        return 1;

    switch (model->type)
    {
        case goldsrc::mod_sprite:
            count = ((goldsrc::msprite_t*)model->cache.data)->numframes;
        break;

        case goldsrc::mod_studio:
            count = R_StudioBodyVariations(model);
        break;

        default:
            return 1;
    }

    if (count < 1)
        return 1;

    return count;
}
