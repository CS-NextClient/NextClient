#include "../engine.h"
#include <optick.h>
#include <model.h>
#include "../console/console.h"
#include "../graphics/gl_local.h"

mspriteframe_t* R_GetSpriteFrame(msprite_t* pSprite, int frame)
{
    OPTICK_EVENT();

    if (!pSprite)
    {
        Con_Printf("Sprite:  no pSprite!!!\n");
        return nullptr;
    }

    if (!pSprite->numframes)
    {
        Con_Printf("Sprite:  pSprite has no frames!!!\n");
        return nullptr;
    }

    if (frame >= pSprite->numframes || frame < 0)
        Con_DPrintf(ConLogType::Info, "Sprite: no such frame %d\n", frame);

    if (pSprite->frames[frame].type == SPR_SINGLE)
        return pSprite->frames[frame].frameptr;

    return nullptr;
}
