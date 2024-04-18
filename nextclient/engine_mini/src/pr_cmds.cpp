#include "engine.h"
#include "common/filesystem.h"

qboolean PF_IsMapValid_I(const char *mapname)
{
    char cBuf[42];
	if (!mapname || mapname[0] == '\0')
        return FALSE;

    Q_snprintf(cBuf, sizeof(cBuf), "maps/%.32s.bsp", mapname);
    return FS_FileExists(cBuf);
}

qboolean PF_IsDedicatedServer()
{
    return g_engfuncs.pfnIsDedicatedServer();
}
