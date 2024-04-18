#include "main.h"
#include <cstring>
#include "viewmodel_overrides.h"
#include "fov.h"
#include <maintypes.h>
#include <weapontype.h>
#include <pm_movevars.h>
#include <parsemsg.h>

void V_ResetViewmodelOverrides();
void V_SetViewmodelOverrides(cl_entity_t* view_model);
int V_ViewModelFx_MSG(const char *pszName, int iSize, void *pbuf);
void V_OffsetViewmodel(cl_entity_t *vm, const QAngle& angles);
void V_AddBob(ref_params_t *pparams, vec3_t origin, const QAngle& angles);
void V_AddLag(ref_params_s *pparams, vec3_t origin, const QAngle& angles);
float Map(float value, float low1, float high1, float low2, float high2);

/* values for cl_bobstyle */
enum { BOB_DEFAULT, BOB_OLD, BOB_CSTRIKE15 };

struct
{
    float bobTime;
    float lastBobTime;
    float lastSpeed;
    float vertBob;
    float horBob;
} g_bobVars;

viewmodel_overrides_t viewmodel_overrides;

cvar_t *viewmodel_disable_shift;

cvar_t *viewmodel_offset_x;
cvar_t *viewmodel_offset_y;
cvar_t *viewmodel_offset_z;

cvar_t *cl_bobstyle;

cvar_t *cl_bobcycle;
cvar_t *cl_bobup;
cvar_t *cl_bob;

cvar_t *cl_bobamt_vert;
cvar_t *cl_bobamt_lat;
cvar_t *cl_bob_lower_amt;

cvar_t *cl_rollangle;
cvar_t *cl_rollspeed;

cvar_t *viewmodel_lag_scale;
cvar_t *viewmodel_lag_speed;

void ViewInit()
{
    viewmodel_disable_shift = gEngfuncs.pfnRegisterVariable("viewmodel_disable_shift", "0", FCVAR_ARCHIVE);

    viewmodel_offset_x = gEngfuncs.pfnRegisterVariable("viewmodel_offset_x", "0", FCVAR_ARCHIVE);
    viewmodel_offset_y = gEngfuncs.pfnRegisterVariable("viewmodel_offset_y", "0", FCVAR_ARCHIVE);
    viewmodel_offset_z = gEngfuncs.pfnRegisterVariable("viewmodel_offset_z", "0", FCVAR_ARCHIVE);

    cl_bobstyle = gEngfuncs.pfnRegisterVariable("cl_bobstyle", "0", FCVAR_ARCHIVE);

    cl_bobcycle = gEngfuncs.pfnGetCvarPointer("cl_bobcycle");
    cl_bobup = gEngfuncs.pfnGetCvarPointer("cl_bobup");
    cl_bob = gEngfuncs.pfnGetCvarPointer("cl_bob");

    cl_bobcycle->flags |= FCVAR_ARCHIVE;
    cl_bobup->flags |= FCVAR_ARCHIVE;

    cl_bobamt_vert = gEngfuncs.pfnRegisterVariable("cl_bobamt_vert", "0.13", FCVAR_ARCHIVE);
    cl_bobamt_lat = gEngfuncs.pfnRegisterVariable("cl_bobamt_lat", "0.32", FCVAR_ARCHIVE);
    cl_bob_lower_amt = gEngfuncs.pfnRegisterVariable("cl_bob_lower_amt", "8", FCVAR_ARCHIVE);

    cl_rollangle = gEngfuncs.pfnRegisterVariable("cl_rollangle", "0", FCVAR_ARCHIVE);
    cl_rollspeed = gEngfuncs.pfnRegisterVariable("cl_rollspeed", "200", FCVAR_ARCHIVE);

    viewmodel_lag_scale = gEngfuncs.pfnRegisterVariable("viewmodel_lag_scale", "0", FCVAR_ARCHIVE);
    viewmodel_lag_speed = gEngfuncs.pfnRegisterVariable("viewmodel_lag_speed", "8.0", FCVAR_ARCHIVE);

    gEngfuncs.pfnHookUserMsg("ViewModelFx", V_ViewModelFx_MSG);
}

void ViewVidInit()
{
    V_ResetViewmodelOverrides();
}

void ViewCalcRefdef(ref_params_s *pparams, V_CalcRefdefNext next)
{
    // we should not change movevars if we are a server (this is possible when running through New Game), because this will cause a buffer overflow due to SVC_NEWMOVEVARS
    if (!sv_static->dll_initialized || !sv_static->clients->active)
    {
        pparams->movevars->rollangle = cl_rollangle->value;
        pparams->movevars->rollspeed = cl_rollspeed->value;
    }

    if ((int)cl_bobstyle->value == BOB_CSTRIKE15)
    {
        float bobcycle, bobup, bob;

        bobcycle = cl_bobcycle->value;
        bobup = cl_bobup->value;
        bob = cl_bob->value;

        cl_bobcycle->value = 0;
        cl_bobup->value = 0;
        cl_bob->value = 0;

        next->Invoke(pparams);

        cl_bobcycle->value = bobcycle;
        cl_bobup->value = bobup;
        cl_bob->value = bob;
    }
    else
    {
        next->Invoke(pparams);
    }

    if (!pparams->intermission && !pparams->paused)
    {
        cl_entity_t *view_model = gEngfuncs.GetViewModel();

        QAngle view_model_angle(view_model->angles);
        QAngle ref_view_angle(pparams->viewangles);

        V_OffsetViewmodel(view_model, ref_view_angle);

        if ((int)viewmodel_disable_shift->value == 1)
        {
            matrix3x4_t final_transform;

            // remove shift from origin
            view_model->origin[2] += 1;

            // make a translate to z -1
            matrix3x4_t translate;
            SetIdentityMatrix(translate);
            PositionMatrix(Vector(0, 0, -1), translate);

            // make rotate on view angle
            matrix3x4_t rotate;
            std::memset(rotate.Base(), 0, sizeof(rotate));
            AngleMatrix(ref_view_angle, rotate);

            // combine
            MatrixMultiply(rotate, translate, final_transform);

            // extract origin which is shift for current position
            Vector shift;
            MatrixPosition(final_transform, shift);

            VectorAdd(view_model->origin, shift, view_model->origin);
        }

        if ((int)cl_bobstyle->value == BOB_CSTRIKE15)
        {
            V_AddBob(pparams, view_model->origin, view_model_angle);
        }
        else if ((int)cl_bobstyle->value == BOB_OLD)
        {
            view_model->curstate.angles[0] = view_model_angle[0];
            view_model->curstate.angles[1] = view_model_angle[1];
            view_model->curstate.angles[2] = view_model_angle[2];
        }

        if (viewmodel_lag_scale->value != 0)
            V_AddLag(pparams, view_model->origin, view_model_angle);

        V_SetViewmodelOverrides(view_model);
    }

    FovThink();
}

void V_SetViewmodelOverrides(cl_entity_t* view_model)
{
    if (viewmodel_overrides.rendermode_override)
        view_model->curstate.rendermode = viewmodel_overrides.rendermode;

    if (viewmodel_overrides.renderamt_override)
        view_model->curstate.renderamt = viewmodel_overrides.renderamt;

    if (viewmodel_overrides.color_override)
        view_model->curstate.rendercolor = viewmodel_overrides.color;

    if (viewmodel_overrides.renderfx_override)
        view_model->curstate.renderfx = viewmodel_overrides.renderfx;

    if (viewmodel_overrides.skin_override)
        view_model->curstate.skin = viewmodel_overrides.skin;

    if (viewmodel_overrides.body_override)
        view_model->curstate.body = viewmodel_overrides.body;
}

int V_ViewModelFx_MSG(const char *pszName, int iSize, void *pbuf)
{
    enum class SET_MODE { NONE, SET, RESET };

    // 8 bit  6 bit: what is set, 1 bit contains reset byte, 1 bit reserved  (from lower to higher)
    // 8 bit  6 bit: what is reset, 2 bit reserved (from lower to higher)
    // 8 bit  3 bit: rendermode, 5 bit: renderfx (from lower to higher)
    // 8 bit  renderamt
    // 24 bit rendercolor
    // 8 bit  skin
    // 32 bit body

    BEGIN_READ(pbuf, iSize);

    int what = READ_BYTE();
    if (what == -1)
    {
        V_ResetViewmodelOverrides();
        return 1;
    }

    SET_MODE mode_mode  = what & (1 << 0) ? SET_MODE::SET : SET_MODE::NONE;
    SET_MODE mode_fx    = what & (1 << 1) ? SET_MODE::SET : SET_MODE::NONE;
    SET_MODE mode_amt   = what & (1 << 2) ? SET_MODE::SET : SET_MODE::NONE;
    SET_MODE mode_color = what & (1 << 3) ? SET_MODE::SET : SET_MODE::NONE;
    SET_MODE mode_skin  = what & (1 << 4) ? SET_MODE::SET : SET_MODE::NONE;
    SET_MODE mode_body  = what & (1 << 5) ? SET_MODE::SET : SET_MODE::NONE;

    if (what & (1 << 6))
    {
        int what_reset = READ_BYTE();
        if (what_reset == -1)
        {
            V_ResetViewmodelOverrides();
            return 1;
        }

        mode_mode  = what_reset & (1 << 0) ? SET_MODE::RESET : mode_mode;
        mode_fx    = what_reset & (1 << 1) ? SET_MODE::RESET : mode_fx;
        mode_amt   = what_reset & (1 << 2) ? SET_MODE::RESET : mode_amt;
        mode_color = what_reset & (1 << 3) ? SET_MODE::RESET : mode_color;
        mode_skin  = what_reset & (1 << 4) ? SET_MODE::RESET : mode_skin;
        mode_body  = what_reset & (1 << 5) ? SET_MODE::RESET : mode_body;
    }

    if (mode_mode == SET_MODE::SET || mode_fx == SET_MODE::SET)
    {
        int mode_and_fx = READ_BYTE();
        if (mode_and_fx == -1)
        {
            V_ResetViewmodelOverrides();
            return 1;
        }

        if (mode_mode == SET_MODE::SET)
        {
            int rendermode = mode_and_fx & 0b111;
            rendermode = std::clamp(rendermode, 0, (int)kRenderTransAdd);

            viewmodel_overrides.rendermode = rendermode;
            viewmodel_overrides.rendermode_override = true;
        }

        if (mode_fx == SET_MODE::SET)
        {
            int renderfx = (mode_and_fx >> 3) & 0b11111;
            renderfx = std::clamp(renderfx, 0, (int)kRenderFxLightMultiplier);

            viewmodel_overrides.renderfx = renderfx;
            viewmodel_overrides.renderfx_override = true;
        }
    }
    else
    {
        if (mode_mode == SET_MODE::RESET)
            viewmodel_overrides.rendermode_override = false;

        if (mode_fx == SET_MODE::RESET)
            viewmodel_overrides.renderfx_override = false;
    }

    if (mode_amt == SET_MODE::SET)
    {
        int renderamt = READ_BYTE();
        if (renderamt == -1)
        {
            V_ResetViewmodelOverrides();
            return 1;
        }

        viewmodel_overrides.renderamt = renderamt;
        viewmodel_overrides.renderamt_override = true;
    }
    else if (mode_amt == SET_MODE::RESET)
    {
        viewmodel_overrides.renderamt_override = false;
    }

    if (mode_color == SET_MODE::SET)
    {
        int color[3];
        color[0] = READ_BYTE();
        color[1] = READ_BYTE();
        color[2] = READ_BYTE();

        if (color[0] == -1 || color[1] == -1 || color[2] == -1)
        {
            V_ResetViewmodelOverrides();
            return 1;
        }

        viewmodel_overrides.color = color24 { (byte)color[0], (byte)color[1], (byte)color[2] };
        viewmodel_overrides.color_override = true;
    }
    else if (mode_color == SET_MODE::RESET)
    {
        viewmodel_overrides.skin_override = false;
    }

    if (mode_skin == SET_MODE::SET)
    {
        int skin = READ_BYTE();
        if (skin == -1)
        {
            V_ResetViewmodelOverrides();
            return 1;
        }

        viewmodel_overrides.skin = skin;
        viewmodel_overrides.skin_override = true;
    }
    else if (mode_skin == SET_MODE::RESET)
    {
        viewmodel_overrides.skin_override = false;
    }

    if (mode_body == SET_MODE::SET)
    {
        int body = READ_LONG();
        if (body == -1)
        {
            V_ResetViewmodelOverrides();
            return 1;
        }

        viewmodel_overrides.body = body;
        viewmodel_overrides.body_override = true;
    }
    else if (mode_body == SET_MODE::RESET)
    {
        viewmodel_overrides.body_override = false;
    }

    return 1;
}


void V_CalcBob(ref_params_t *pparams)
{
    float speed;
    float maxSpeedDelta;
    float lowerAmt;
    float bobOffset;
    float bobCycle;
    float bobScale;
    float cycle;

    if (pparams->frametime == 0)
        return;

    speed = Vector2DLength(pparams->simvel);

    maxSpeedDelta = std::max(0.0f, (pparams->time - g_bobVars.lastBobTime) * 620);

    speed = std::clamp(speed, g_bobVars.lastSpeed - maxSpeedDelta, g_bobVars.lastSpeed + maxSpeedDelta);
    speed = std::clamp(speed, -320.0f, 320.0f);

    g_bobVars.lastSpeed = speed;

    lowerAmt = cl_bob_lower_amt->value * (speed * 0.001f);

    bobOffset = Map(speed, 0, 320, 0, 1);

    g_bobVars.bobTime += (pparams->time - g_bobVars.lastBobTime) * bobOffset;
    g_bobVars.lastBobTime = pparams->time;

    /* scale the bob by 1.25, this wasn't in 10040 but this way
    cs 1.6's default cl_bobcycle value (0.8) will look right */
    bobCycle = (((1000.0f - 150.0f) / 3.5f) * 0.001f) * cl_bobcycle->value * 1.25f;

    cycle = g_bobVars.bobTime - (int)(g_bobVars.bobTime / bobCycle) * bobCycle;
    cycle /= bobCycle;

    if (cycle < cl_bobup->value)
        cycle = M_PI * cycle / cl_bobup->value;
    else
        cycle = M_PI + M_PI * (cycle - cl_bobup->value) / (1.0f - cl_bobup->value);

    bobScale = 0.00625f;

    if (!pparams->onground)
        bobScale = 0.00125f;

    g_bobVars.vertBob = speed * (bobScale * cl_bobamt_vert->value);
    g_bobVars.vertBob = (g_bobVars.vertBob * 0.3f + g_bobVars.vertBob * 0.7f * sin(cycle));
    g_bobVars.vertBob = std::clamp(g_bobVars.vertBob - lowerAmt, -8.0f, 4.0f);

    cycle = g_bobVars.bobTime - (int)(g_bobVars.bobTime / bobCycle * 2) * bobCycle * 2;
    cycle /= bobCycle * 2;

    if (cycle < cl_bobup->value)
        cycle = M_PI * cycle / cl_bobup->value;
    else
        cycle = M_PI + M_PI * (cycle - cl_bobup->value) / (1.0f - cl_bobup->value);

    g_bobVars.horBob = speed * (bobScale * cl_bobamt_lat->value);
    g_bobVars.horBob = g_bobVars.horBob * 0.3f + g_bobVars.horBob * 0.7f * sin( cycle);
    g_bobVars.horBob = std::clamp(g_bobVars.horBob, -7.0f, 4.0f);
}

void V_AddBob(ref_params_t *pparams, vec3_t origin, const QAngle& angles)
{
    Vector forward, right;

    V_CalcBob(pparams);

    AngleVectors(angles, &forward, &right, nullptr);

    VectorMultiply(forward, g_bobVars.vertBob * 0.4f, forward);
    origin[0] += forward[0];
    origin[1] += forward[1];
    origin[2] -= forward[2];

    origin[2] += g_bobVars.vertBob * 0.1f;

    VectorMultiply(right, g_bobVars.horBob * 0.2f, right);
    origin[0] += right[0];
    origin[1] += right[1];
    origin[2] -= right[2];
}

void V_AddLag(ref_params_s *pparams, vec3_t origin, const QAngle& angles)
{
    Vector forward;
    Vector delta_angles;
    static Vector last_angles;

    AngleVectors(angles, &forward, nullptr, nullptr);

    delta_angles[0] = forward[0] - last_angles[0];
    delta_angles[1] = forward[1] - last_angles[1];
    delta_angles[2] = forward[2] - last_angles[2];

    last_angles.MulAdd(last_angles, delta_angles, viewmodel_lag_speed->value * pparams->frametime);

    VectorMultiply(delta_angles, -1 * viewmodel_lag_scale->value, delta_angles);
    origin[0] += delta_angles[0];
    origin[1] += delta_angles[1];
    origin[2] -= delta_angles[2];
}

void V_OffsetViewmodel(cl_entity_t *vm, const QAngle& angles)
{
    Vector forward, right, up;
    float x, y, z;

    AngleVectors(angles, &forward, &right, &up);

    if (g_LastPlayerState.client.m_iId == WEAPON_KNIFE)
        x = -viewmodel_offset_x->value;
    else
        x = viewmodel_offset_x->value;

    y = viewmodel_offset_y->value;
    z = viewmodel_offset_z->value;

    Vector vm_origin(vm->origin);
    vm_origin.MulAdd(vm_origin, right, x);
    vm_origin.MulAdd(vm_origin, forward, y);
    vm_origin.MulAdd(vm_origin, up, z);
    vm_origin.CopyToArray(vm->origin);
}

void V_ResetViewmodelOverrides()
{
    std::memset(&viewmodel_overrides, 0, sizeof(viewmodel_overrides));
}

float Map(float value, float low1, float high1, float low2, float high2)
{
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}
