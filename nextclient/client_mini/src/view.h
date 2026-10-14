#pragma once

#include <goldsrc/view.h>

#include "main.h"

// Local view origin/angles captured from the last main-view ViewCalcRefdef pass (spectator picture-in-picture passes excluded).
extern vec3_t g_MainViewOrigin;
extern vec3_t g_MainViewAngles;

void ViewInit();
void ViewVidInit();
void ViewCalcRefdef(goldsrc::ref_params_t* pparams, V_CalcRefdefNext next);