#pragma once

void PluginBridge_Init();
void PluginBridge_Shutdown();
void PluginBridge_Reset();
void PluginBridge_PredictionReady();
void PluginBridge_Draw(float time, int intermission);
void PluginBridge_Frame(double time);
void PluginBridge_Voice(int index, int talking);

void PluginBridge_Prepare();
void PluginBridge_WrapMessages();
