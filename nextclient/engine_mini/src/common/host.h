#pragma once

#include "engine.h"

qboolean Host_IsSinglePlayerGame();
int Host_GetMaxClients();
qboolean Host_FilterTime(float time);
void Host_WriteConfiguration();
