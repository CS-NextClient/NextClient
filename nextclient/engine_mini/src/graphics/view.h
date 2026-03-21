#pragma once
#include "engine.h"

extern cvar_t* v_dark_cvar;
extern cvar_t* crosshair_cvar;
extern cvar_t* gamma_cvar;
extern cvar_t* lightgamma_cvar;
extern cvar_t* texgamma_cvar;
extern cvar_t* brightness_cvar;
extern cvar_t* lambert_cvar;
extern cvar_t* direct_cvar;

void V_Init_AfterCvarsHook();
