#include <cstring>

#include "main.h"

#include <parsemsg.h>
#include <maintypes.h>
#include <pm_movevars.h>

#include "viewmodel_overrides.h"
#include "fov.h"
#include "camera.h"

static void V_ResetViewmodelOverrides();
static void V_SetViewmodelOverrides(cl_entity_t* view_model);
static int V_ViewModelFx_MSG(const char *pszName, int iSize, void *pbuf);

static viewmodel_overrides_t viewmodel_overrides;

static cvar_t *viewmodel_disable_shift;

static cvar_t *viewmodel_offset_x;
static cvar_t *viewmodel_offset_y;
static cvar_t *viewmodel_offset_z;

static cvar_t *cl_bobstyle;

static cvar_t *cl_bobcycle;
static cvar_t *cl_bobup;
static cvar_t *cl_bob;

static cvar_t *cl_bobamt_vert;
static cvar_t *cl_bobamt_lat;
static cvar_t *cl_bob_lower_amt;
static cvar_t *cl_bob_camera;

static cvar_t *cl_rollangle;
static cvar_t *cl_rollspeed;

static cvar_t *viewmodel_lag_style;
static cvar_t *viewmodel_lag_scale;
static cvar_t *viewmodel_lag_speed;

static cvar_t *spec_pip;

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
    cl_bob_camera = gEngfuncs.pfnRegisterVariable("cl_bob_camera", "1", FCVAR_ARCHIVE);

    cl_rollangle = gEngfuncs.pfnRegisterVariable("cl_rollangle", "0", FCVAR_ARCHIVE);
    cl_rollspeed = gEngfuncs.pfnRegisterVariable("cl_rollspeed", "200", FCVAR_ARCHIVE);

    viewmodel_lag_style = gEngfuncs.pfnRegisterVariable("viewmodel_lag_style", "0", FCVAR_ARCHIVE);
    viewmodel_lag_scale = gEngfuncs.pfnRegisterVariable("viewmodel_lag_scale", "1.0", FCVAR_ARCHIVE);
    viewmodel_lag_speed = gEngfuncs.pfnRegisterVariable("viewmodel_lag_speed", "8.0", FCVAR_ARCHIVE);

    spec_pip = gEngfuncs.pfnGetCvarPointer("spec_pip");

    gEngfuncs.pfnHookUserMsg("ViewModelFx", V_ViewModelFx_MSG);
}

void ViewVidInit()
{
    V_ResetViewmodelOverrides();
}

struct
{
    float bobTime;
    float lastBobTime;
    float lastSpeed;
    float vertBob;
    float horBob;
} g_bobVars;

static float Map(float value, float low1, float high1, float low2, float high2)
{
	return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

static void V_CalcBob_CSGO(ref_params_t *pparams)
{
	float speed;
	float maxSpeedDelta;
	float lowerAmt;
	float bobOffset;
	float bobCycle;
	float bobScale;
	float cycle;

	speed = Vector2DLength(pparams->simvel);

	maxSpeedDelta = std::max(0.f, (pparams->time - g_bobVars.lastBobTime) * 620.f);

	speed = std::clamp(speed, g_bobVars.lastSpeed - maxSpeedDelta, g_bobVars.lastSpeed + maxSpeedDelta);
	speed = std::clamp(speed, -320.f, 320.f);

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
		cycle = M_PI_F * cycle / cl_bobup->value;
	else
		cycle = M_PI_F + M_PI_F * (cycle - cl_bobup->value) / (1.0f - cl_bobup->value);

	bobScale = 0.00625f;

	if (!pparams->onground)
		bobScale = 0.00125f;

	g_bobVars.vertBob = speed * (bobScale * cl_bobamt_vert->value);
	g_bobVars.vertBob = (g_bobVars.vertBob * 0.3f + g_bobVars.vertBob * 0.7f * sinf(cycle));
	g_bobVars.vertBob = std::clamp(g_bobVars.vertBob - lowerAmt, -8.f, 4.f);

	cycle = g_bobVars.bobTime - (int)(g_bobVars.bobTime / bobCycle * 2) * bobCycle * 2;
	cycle /= bobCycle * 2;

	if (cycle < cl_bobup->value)
		cycle = M_PI_F * cycle / cl_bobup->value;
	else
		cycle = M_PI_F + M_PI_F * (cycle - cl_bobup->value) / (1.0f - cl_bobup->value);

	g_bobVars.horBob = speed * (bobScale * cl_bobamt_lat->value);
	g_bobVars.horBob = g_bobVars.horBob * 0.3f + g_bobVars.horBob * 0.7f * sinf(cycle);
	g_bobVars.horBob = std::clamp(g_bobVars.horBob, -7.f, 4.f);
}

static void V_AddBob_CSGO(ref_params_t *pparams, vec3_t origin, vec3_t front, vec3_t side)
{
    V_CalcBob_CSGO(pparams);

    VectorMA_2(front, g_bobVars.vertBob * 0.4f, origin);

    origin[2] += g_bobVars.vertBob * 0.1f;

    VectorMA_2(side, g_bobVars.horBob * 0.2f, origin);
}

static float V_CalcBob(ref_params_t *pparams)
{
    static double bobtime;

    bobtime += pparams->frametime;

    float cycle = (float)(bobtime - (int)(bobtime / cl_bobcycle->value) * cl_bobcycle->value);
    cycle /= cl_bobcycle->value;

    if (cycle < cl_bobup->value)
        cycle = M_PI_F * cycle / cl_bobup->value;
    else
        cycle = M_PI_F + M_PI_F * (cycle - cl_bobup->value) / (1.0f - cl_bobup->value);

    float bob = Vector2DLength(pparams->simvel) * cl_bob->value;
    bob = bob * 0.3f + bob * 0.7f * sinf(cycle);
    return std::clamp(bob, -7.f, 4.f);
}

static void V_AddLag_HL2(ref_params_t *pparams, vec3_t origin, vec3_t front)
{
	vec3_t delta_front;
	static vec3_t last_front;

	delta_front[0] = front[0] - last_front[0];
	delta_front[1] = front[1] - last_front[1];
	delta_front[2] = front[2] - last_front[2];

	VectorMA(delta_front, viewmodel_lag_speed->value * pparams->frametime, last_front);
	VectorMA_2(delta_front, -1 * viewmodel_lag_scale->value, origin);
}

// quite an inefficent way to do css style lag but oh well
typedef struct
{
	bool valid;
	vec3_t value;
} lag_angles_t;

#define STEP_MILLIS 10
#define ANGLE_BACKUP 128

#define TO_STEP(x) ((int)((x) * STEP_MILLIS))
#define FROM_STEP(x) ((float)(x) * (1.0f / STEP_MILLIS))

static lag_angles_t lag_angles[ANGLE_BACKUP];
static int last_step;

static void AddLagAngles(float time, vec3_t angles)
{
	int step = TO_STEP(time);

	if (step < last_step)
	{
		// client restart
		memset(lag_angles, 0, sizeof(lag_angles));
	}
	else if (step == last_step)
	{
		// too soon
		return;
	}

	last_step = step;

	VectorCopy(angles, lag_angles[step % ANGLE_BACKUP].value);
	lag_angles[step % ANGLE_BACKUP].valid = true;
}

static bool GetLagAngles(float time, vec3_t dest)
{
	int step = TO_STEP(time);

	lag_angles_t *angles = &lag_angles[step % ANGLE_BACKUP];
	if (!angles->valid)
		return false;

	float step_time = FROM_STEP(TO_STEP(time));
	if (step_time < time)
	{
		lag_angles_t *next_angles = &lag_angles[(step + 1) % ANGLE_BACKUP];
		if (!next_angles->valid)
		{
			// can't lerp to these
			VectorCopy(angles->value, dest);
		}
		else
		{
			float next_time = FROM_STEP(step + 1);
			float frac = Map(time, step_time, next_time, 0, 1);
			AngleLerp(angles->value, next_angles->value, frac, dest);
		}
	}
	else
	{
		// if round_time > time, the difference is
		// probably so small that it can't be noticed
		VectorCopy(angles->value, dest);
	}

	return true;
}

static void V_AddLag_CSS(ref_params_t *pparams, vec3_t origin, vec3_t angles, vec3_t front, vec3_t side, vec3_t up)
{
	AddLagAngles(pparams->time, angles);

	vec3_t prev_angles;
	if (!GetLagAngles(pparams->time - 0.1f, prev_angles))
		return;

	vec3_t delta_angles;
	VectorSubtract(prev_angles, angles, delta_angles);
	VectorNegate(delta_angles);

	Vector delta_front;
	AngleVectors(QAngle(delta_angles), &delta_front, nullptr, nullptr);

	VectorMA_2(front, (1 - delta_front[0]) * viewmodel_lag_scale->value, origin);
	VectorMA_2(side, (delta_front[1]) * viewmodel_lag_scale->value, origin);
	VectorMA_2(up, (-delta_front[2]) * viewmodel_lag_scale->value, origin);
}

static void V_OffsetViewmodel(cl_entity_t *vm, vec3_t front, vec3_t side, vec3_t up)
{
    vec3_t extra_origin;
    VectorClear(extra_origin);

    float x = viewmodel_offset_x->value + extra_origin[0];
    float y = viewmodel_offset_y->value + extra_origin[1];
    float z = -(viewmodel_offset_z->value + extra_origin[2]);

    if (g_GameHud->cl_righthand->value == 0)
        x = -x;

    VectorMA_2(side, x, vm->origin);
    VectorMA_2(front, y, vm->origin);
    VectorMA_2(up, z, vm->origin);
}

static void CalcCustomRefdef(ref_params_t *pparams)
{
    cl_entity_t *vm;
    Vector front, side, up;

    vm = gEngfuncs.GetViewModel();
    AngleVectors(QAngle(vm->angles), &front, &side, &up);

    /* fix the slight difference between view and vm origin */
    vm->origin[0] += 1.0f / 32;
    vm->origin[1] += 1.0f / 32;
    vm->origin[2] += 1.0f / 32;

    V_OffsetViewmodel(vm, front, side, up);

    if ((int)viewmodel_disable_shift->value == 1)
    {
        // remove shift from origin
        vm->origin[2] += 1;

        // offset the viewmodel
        VectorMA_2(up, 1, vm->origin);
    }

    if ((int)cl_bobstyle->value == 2)
    {
        V_AddBob_CSGO(pparams, vm->origin, front, side);
    }
    else
    {
        float bob = V_CalcBob(pparams);
        VectorMA_2(front, bob * 0.4f, vm->origin);

        if ((int)cl_bobstyle->value == 1)
        {
            vm->curstate.angles[0] -= bob * 0.3f;
            vm->curstate.angles[1] -= bob * 0.5f;
            vm->curstate.angles[2] -= bob;
        }

        if (cl_bob_camera->value)
        {
            pparams->vieworg[2] += bob;
            vm->origin[2] += bob;
        }
    }

    switch ((int)viewmodel_lag_style->value)
    {
    case 1:
        V_AddLag_HL2(pparams, vm->origin, front);
        break;

    case 2:
        V_AddLag_CSS(pparams, vm->origin, vm->angles, front, side, up);
        break;
    }

    V_SetViewmodelOverrides(vm);

    /* mikkotodo move? */
    CameraApplyMovement(pparams);
}

static void V_GetInEyePos(int target, vec_t *origin, vec_t *angles)
{
    if (!target)
    {
        /* just hope this doesn't happen? */
        return;
    }

    cl_entity_t *ent = gEngfuncs.GetEntityByIndex(target);
    if (!ent)
        return;

    VectorCopy(ent->origin, origin);
    VectorCopy(ent->angles, angles);

    angles[0] *= -3;

    if (!ent->curstate.solid)
    {
        angles[2] = 80;
        origin[2] -= 8;
    }
    else if (ent->curstate.usehull == 1)
        origin[2] += 12;
    else
        origin[2] += 28;
}

static void CalcSpectatorRefdef(ref_params_t *pparams, int nextView)
{
    static vec3_t velocity;

    cl_entity_t *ent = gEngfuncs.GetEntityByIndex(g_iUser2);

    if ((g_iUser1 == INSET_MAP_CHASE || spec_pip->value == INSET_IN_EYE) && ent)
    {
        float timeDiff = ent->curstate.msg_time - ent->prevstate.msg_time;
        if (timeDiff > 0)
        {
            vec3_t distance;
            VectorSubtract(ent->prevstate.origin, ent->curstate.origin, distance);
            VectorScale(distance, 1.0f / timeDiff, distance);

            velocity[0] = velocity[0] * 0.9f + distance[0] * 0.1f;
            velocity[1] = velocity[1] * 0.9f + distance[1] * 0.1f;
            velocity[2] = velocity[2] * 0.9f + distance[2] * 0.1f;
            VectorCopy(velocity, pparams->simvel);
        }

        if (gEngfuncs.IsSpectateOnly() || spec_pip->value == INSET_IN_EYE)
        {
            V_GetInEyePos(g_iUser2, pparams->simorg, pparams->cl_viewangles);
        }
        else
        {
            VectorCopy(ent->angles, pparams->cl_viewangles);
            pparams->cl_viewangles[0] *= -3;
        }
    }

    if (nextView == 0)
    {
        if (g_iUser1 == INSET_MAP_CHASE)
            CalcCustomRefdef(pparams);
    }
    else if (spec_pip->value == INSET_IN_EYE)
    {
        CalcCustomRefdef(pparams);
    }
}

void ViewCalcRefdef(ref_params_s *pparams, V_CalcRefdefNext next)
{
    float old_rollangle, old_rollspeed;

    /* temporarily override view roll settings with client dictated ones */
    old_rollangle = pparams->movevars->rollangle;
    old_rollspeed = pparams->movevars->rollspeed;
    pparams->movevars->rollangle = cl_rollangle->value;
    pparams->movevars->rollspeed = cl_rollspeed->value;

    /* save this off so we can properly handle pip (gets modified by original CalcRefdef) */
    int nextView = pparams->nextView;

    /* we calculate the bob ourselves so zero out the cvar */
    {
        float bob;

        bob = cl_bob->value;
        cl_bob->value = 0;
        next->Invoke(pparams);
        cl_bob->value = bob;
    }

    /* view roll has been applied, restore the settings */
    pparams->movevars->rollangle = old_rollangle;
    pparams->movevars->rollspeed = old_rollspeed;

    if (!pparams->intermission)
    {
        if (pparams->spectator || g_iUser1)
            CalcSpectatorRefdef(pparams, nextView);
        else if (!pparams->paused)
            CalcCustomRefdef(pparams);
    }

    FovThink();
}

static void V_SetViewmodelOverrides(cl_entity_t* view_model)
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

static int V_ViewModelFx_MSG(const char *pszName, int iSize, void *pbuf)
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
        viewmodel_overrides.color_override = false;
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

static void V_ResetViewmodelOverrides()
{
    std::memset(&viewmodel_overrides, 0, sizeof(viewmodel_overrides));
}
