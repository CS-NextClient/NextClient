#include <goldsrc/cvar.h>

#include "engine.h"

#include "view.h"
#include "common/cvar.h"

goldsrc::cvar_t* v_dark_cvar;
goldsrc::cvar_t* crosshair_cvar;
goldsrc::cvar_t* gamma_cvar;
goldsrc::cvar_t* lightgamma_cvar;
goldsrc::cvar_t* texgamma_cvar;
goldsrc::cvar_t* brightness_cvar;
goldsrc::cvar_t* lambert_cvar;
goldsrc::cvar_t* direct_cvar;

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
