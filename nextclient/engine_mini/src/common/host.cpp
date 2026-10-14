#include "host.h"

#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/types.h>

double oldrealtime;

goldsrc::qboolean Host_IsSinglePlayerGame()
{
	if (g_psv->active)
		return g_psvs->maxclients == 1;

	return cl->maxclients == 1;
}

goldsrc::qboolean Host_IsServerActive()
{
    return g_psv->active;
}

int Host_GetMaxClients()
{
    if (g_psv->active)
        return g_psvs->maxclients;

    return cl->maxclients;
}

goldsrc::qboolean Host_FilterTime(float time)
{
	float fps;
	static int command_line_ticrate = -1;

	if (host_framerate->value > 0.0f)
	{
		if (Host_IsSinglePlayerGame() || cls->demoplayback)
		{
			*host_frametime = sys_timescale->value * host_framerate->value;
			*realtime += *host_frametime;
			return true;
		}
	}

	*realtime += sys_timescale->value * time;
	if (g_bIsDedicatedServer)
	{
		if (command_line_ticrate == -1)
			command_line_ticrate = COM_CheckParm("-sys_ticrate");

		if (command_line_ticrate > 0)
			fps = Q_atof(com_argv[command_line_ticrate + 1]);
		else
			fps = sys_ticrate->value;

		if (fps > 0.0f)
		{
			if (1.0f / (fps + 1.0f) > *realtime - oldrealtime)
				return false;
		}
	}
	else
	{
		fps = 100.0f;
		if (g_psv->active || cls->state == goldsrc::ca_disconnected || cls->state == goldsrc::ca_active)
		{
			fps = 0.5f;
			if (fps_max->value >= 0.5f)
				fps = fps_max->value;
		}
		if (!fps_override->value)
		{
			if (fps > 100.0f)
				fps = 100.0f;
		}
		if (cl->maxclients > 1)
		{
			if (fps < 20.0f)
				fps = 20.0f;
		}
		if (gl_vsync->value)
		{
			if (!fps_override->value)
				fps = 100.f;
		}
		if (!cls->timedemo)
		{
			if (sys_timescale->value / (fps + 0.5f) > *realtime - oldrealtime)
				return false;
		}
	}

	*host_frametime = *realtime - oldrealtime;
	oldrealtime = *realtime;

	if (*host_frametime > 0.25f)
		*host_frametime = 0.25f;

	return true;
}

void Host_WriteConfiguration()
{
	eng()->Host_WriteConfiguration.InvokeChained();
}

void Host_ShutdownServer(goldsrc::qboolean crash)
{
    eng()->Host_ShutdownServer.InvokeChained(crash);    
}
