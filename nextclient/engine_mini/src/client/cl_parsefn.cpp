#include "cl_parsefn.h"

#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>

void CL_Parse_Timescale()
{
	auto value = MSG_ReadFloat();

	if (value >= 0.1 && (
			(sv_cheats->value && developer->value == 2)
			|| cls->demoplayback
			|| cls->spectator
		))
	{
		sys_timescale->value = value;
	}
	else
	{
		sys_timescale->value = 1.0;
	}
}
