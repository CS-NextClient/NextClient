#pragma once
#include <goldsrc/types.h>
#include <goldsrc/view.h>

#include "../hlsdk.h"

void AddStartupTiming(const char *name);
const char* CL_CleanFileName(const char* filename);
void CL_ConnectClient();
bool CL_PrecacheResources();
void CL_ConnectionlessPacket();
goldsrc::qboolean CL_IsDevOverviewMode();
void CL_SetDevOverView(goldsrc::refdef_t* refdef);
void CL_Send_CvarValue();

void CL_ClearClientState();
void CL_Disconnect();
void CL_StopPlayback();
void SetCareerAudioState(int state);
