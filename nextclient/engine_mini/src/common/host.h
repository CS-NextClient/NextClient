#pragma once

#include "engine.h"

qboolean Host_IsSinglePlayerGame();
qboolean Host_IsServerActive();
int Host_GetMaxClients();
qboolean Host_FilterTime(float time);
void Host_WriteConfiguration();
void Host_ShutdownServer(qboolean crash);
