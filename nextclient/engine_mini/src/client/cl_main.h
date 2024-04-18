#pragma once
#include "../hlsdk.h"

void AddStartupTiming(const char *name);
const char* CL_CleanFileName(const char* filename);
void CL_ConnectClient();
bool CL_PrecacheResources();
void CL_ConnectionlessPacket();
qboolean CL_IsDevOverviewMode();
void CL_SetDevOverView(refdef_t* refdef);
void CL_Send_CvarValue();
