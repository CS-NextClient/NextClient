#pragma once

#include <memory>
#include <vector>

namespace nitroapi
{
    class NitroApiInterface;
    class Unsubscriber;
} // namespace nitroapi

class RadarBroadcast;
class SmokeRegistry;
class RadarScoreboard;
struct RadarBox;

struct RadarSinks
{
    SmokeRegistry* smoke;
    RadarBroadcast* broadcast;
};

void RadarHooks_SetSinks(const RadarSinks& sinks);
void RadarHooks_ClearSinks();

// Call once the client table exists (HUD_Init); no-op if already patched.
void RadarHooks_PatchHudText();

void RadarHooks_UnpatchHudText();

// Call at the end of the stock HUD pass, after the hooked glyph draws.
void RadarHooks_FlushText();

// Call on VidInit: a video mode or UI scale change rebuilds the fonts.
void RadarHooks_InvalidateGlyphMetrics();

bool RadarHooks_IsScoreboardVisible();
void RadarHooks_UpdateScoreboard(RadarScoreboard& placement, const RadarBox* radar);

void RadarHooks_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub);
