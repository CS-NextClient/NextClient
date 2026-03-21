#pragma once

void DT_Initialize();
void DT_LoadDetailMapFile(const char* level_name);
void DT_LoadDetailTexture(const char* diffuseName, int diffuseId);
qboolean DT_SetRenderState(int diffuseId);
