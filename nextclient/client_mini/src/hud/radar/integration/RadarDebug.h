#pragma once

struct RadarBox;

// Diagnostics overlay for the advanced radar (hud_radar_debug).
void RadarDebug_Init();
bool RadarDebug_IsEnabled();

// Call after the radar has finished drawing: the panel's own cost must stay out of the
// numbers it prints.
void RadarDebug_DrawFrame(const RadarBox& box, float total_ms, float blur_ms, float traces_ms, float text_ms, int batches);
