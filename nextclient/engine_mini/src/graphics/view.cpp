#include "engine.h"

#include "view.h"
#include "common/cvar.h"

cvar_t* v_dark_cvar;
cvar_t* crosshair_cvar;
cvar_t* gamma_cvar;
cvar_t* lightgamma_cvar;
cvar_t* texgamma_cvar;
cvar_t* brightness_cvar;
cvar_t* lambert_cvar;
cvar_t* direct_cvar;

void V_Init_AfterCvarsHook()
{
    v_dark_cvar = Cvar_FindVar("v_dark");
    crosshair_cvar = Cvar_FindVar("crosshair");
    gamma_cvar = Cvar_FindVar("gamma");
    lightgamma_cvar = Cvar_FindVar("lightgamma");
    texgamma_cvar = Cvar_FindVar("texgamma");
    brightness_cvar = Cvar_FindVar("brightness");
    lambert_cvar = Cvar_FindVar("lambert");
    direct_cvar = Cvar_FindVar("direct");
}
