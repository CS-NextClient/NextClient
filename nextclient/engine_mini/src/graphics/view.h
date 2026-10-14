#pragma once
#include <goldsrc/cvar.h>

#include "engine.h"

extern goldsrc::cvar_t* v_dark_cvar;
extern goldsrc::cvar_t* crosshair_cvar;
extern goldsrc::cvar_t* gamma_cvar;
extern goldsrc::cvar_t* lightgamma_cvar;
extern goldsrc::cvar_t* texgamma_cvar;
extern goldsrc::cvar_t* brightness_cvar;
extern goldsrc::cvar_t* lambert_cvar;
extern goldsrc::cvar_t* direct_cvar;

void V_Init_AfterCvarsHook();
