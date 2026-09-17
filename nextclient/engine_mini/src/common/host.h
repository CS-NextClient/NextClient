#pragma once

#include <goldsrc/types.h>

#include "engine.h"

goldsrc::qboolean Host_IsSinglePlayerGame();
goldsrc::qboolean Host_IsServerActive();
int Host_GetMaxClients();
goldsrc::qboolean Host_FilterTime(float time);
void Host_WriteConfiguration();
void Host_ShutdownServer(goldsrc::qboolean crash);
