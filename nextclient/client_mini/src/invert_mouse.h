#pragma once
#include "main.h"

void InvertMouseInit();
void ResetInvertMouse();
void CL_CreateMove_InvertMousePre(float frametime, usercmd_s* cmd, int active);
void CL_CreateMove_InvertMousePost(float frametime, usercmd_s* cmd, int active);
