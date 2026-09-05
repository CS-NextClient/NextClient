#pragma once

#include <nitroapi/NitroApiInterface.h>

void FovInit();
void FovThink();
int FovMsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf, UserMsg_SetFOVNext next);
void FovHUD_UpdateClientData(client_data_t *cdata, float flTime, int result);
