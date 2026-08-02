#pragma once

#include <memory>
#include <vector>

namespace nitroapi
{
    class NitroApiInterface;
    class Unsubscriber;
} // namespace nitroapi

// Radar server-message and HUD-text plumbing, kept out of the DLL wiring file:
//  - the stock-client HUD text-draw table patch that relocates text from under the
//    radar (feeds HudTextZone),
//  - the bomb/hostage user-message parsers that feed RadarBroadcast,
//  - the createsmoke event wrap that feeds RadarSmoke.

// Installs the client.dll text-draw table patch. Call once the client table exists
// (HUD_Init). No-op if already patched.
void RadarHooks_PatchHudText();

// Reverts the text-draw table patch. Safe to call when not patched.
void RadarHooks_UnpatchHudText();

// Draws any buffered in-zone glyph line, shifted out from under the radar. Call once at
// the end of the stock HUD pass (after the hooked glyph draws, before the next frame).
void RadarHooks_FlushText();

// Drops the cached glyph advance widths. Call on VidInit: the fonts are rebuilt on a
// video mode or UI scale change, so cached widths would no longer match.
void RadarHooks_InvalidateGlyphMetrics();

// Subscribes the radar message handlers and the createsmoke event wrap, appending
// their unsubscribers to `unsub` so teardown matches the rest of the client.
void RadarHooks_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub);
