#pragma once

struct RadarBox;

// Diagnostics overlay for the advanced radar (hud_radar_debug cvar): per-frame
// draw time, blur time and quad-batch count, with rolling history graphs drawn
// under the radar box.
void RadarDebug_Init();
bool RadarDebug_Enabled();

// Records this frame's numbers and draws the panel. Call after the radar has
// finished drawing so the panel's own cost stays out of the measurements.
void RadarDebug_Frame(const RadarBox& box, float total_ms, float blur_ms, float traces_ms, float text_ms, int batches);
