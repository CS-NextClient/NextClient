#pragma once

// The zone is the square of side `size` at (x, y); stock and server HUD text drawn under
// it is relocated.
void HudTextZone_SetZone(float x, float y, float size);
void HudTextZone_ClearZone();

// Suppresses the shift while NextClient draws its own HUD text.
void HudTextZone_SetPassthrough(bool on);

// Once per frame, before the stock HUD is drawn.
void HudTextZone_BeginFrame(int screen_width, int screen_height);

bool HudTextZone_IsRowInZone(int y);

// Needs the width of a WHOLE line, so the glyph draw hooks buffer a line and call this once
// its full width is known (see RadarHooks); shifting per glyph tears the line apart.
void HudTextZone_ComputeLineShift(int left, int y, int width, int& dx, int& dy);

bool HudTextZone_ShiftString(int& x, int& y, int width);
