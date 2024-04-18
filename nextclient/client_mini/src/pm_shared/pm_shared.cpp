#include "../main.h"

int PM_GetVisEntInfo(int ent)
{
    if (ent >= 0 && ent <= pmove->numvisent)
        return pmove->visents[ent].info;

    return -1;
}

int PM_GetPhysEntInfo(int ent)
{
    if (ent >= 0 && ent <= pmove->numphysent)
        return pmove->physents[ent].info;

    return -1;
}