#pragma once
#include <goldsrc/usercmd.h>

#include "main.h"

void InvertMouseInit();
void ResetInvertMouse();
void CL_CreateMove_InvertMousePre(float frametime, goldsrc::usercmd_t* cmd, int active);
void CL_CreateMove_InvertMousePost(float frametime, goldsrc::usercmd_t* cmd, int active);
