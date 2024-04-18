#pragma once

#include <nitroapi/NitroApiInterface.h>

const float kFovDefault = 90.f;
const float kFovMin = 70.f;
const float kFovMax = 100.f;

void FovInit();
void FovThink();
int FovMsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf, UserMsg_SetFOVNext next);
void FovHUD_UpdateClientData(client_data_t *cdata, float flTime, int result);
