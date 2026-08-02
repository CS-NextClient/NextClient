#pragma once

// Reserves a screen rectangle published by a large HUD element and relocates the
// stock/server HUD text that would otherwise be drawn under it (right of the zone
// when it still fits on screen, otherwise below). Kept independent of the radar so
// any oversized HUD element can publish a zone; the advanced radar is its only
// caller today.
namespace HudTextZone
{
    // The reserved rectangle, published each frame the owning element draws. The
    // passthrough flag suppresses the shift while NextClient draws its own HUD text.
    void SetZone(float x, float y, float size);
    void ClearZone();
    void SetPassthrough(bool on);

    // Called once per frame before the stock HUD is drawn; records the screen size for
    // the fits-to-the-right test.
    void FrameBegin(int screen_width, int screen_height);

    // Cheap pre-check for the draw hooks: true only when a piece drawn at this row
    // could be shifted at all (zone active, not passthrough, row inside the zone's
    // vertical band). When false the caller can skip measuring the text width.
    bool RowInZone(int y);

    // Shift for a whole line of text at (left, y) with the given full width: right of
    // the zone when it still fits on screen, otherwise straight below it. Zero when the
    // line does not overlap the zone. The glyph draw hooks buffer a line and call this
    // once its full width is known (see RadarHooks); the string hooks use TrackText.
    void LineShift(int left, int y, int width, int* dx, int* dy);

    // Shifts a whole-string HUD draw (DrawString/DrawConsoleString) out from under the
    // zone, using the width passed in. The decision is made from this frame's own
    // values (no cross-frame memory). Returns true when a shift was applied.
    bool TrackText(int* x, int* y, int width);
} // namespace HudTextZone
