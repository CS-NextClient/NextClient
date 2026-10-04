#pragma once

void PluginBridgeInit();
void PluginBridgeShutdown();
void PluginBridgeReset();
void PluginBridgePredictionReady();
void PluginBridgeDraw(float time, int intermission);
void PluginBridgeFrame(double time);
void PluginBridgeVoice(int index, int talking);

void PluginBridgePrepare();
void PluginBridgeWrapMessages();
