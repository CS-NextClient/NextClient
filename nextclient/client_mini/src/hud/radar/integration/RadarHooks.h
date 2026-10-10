#pragma once

#include <memory>
#include <string>
#include <vector>

namespace nitroapi
{
    class NitroApiInterface;
    class Unsubscriber;
} // namespace nitroapi

class RadarBroadcast;
class SmokeRegistry;
class RadarScoreboard;
class PlayersLayer;
struct RadarBox;
class RadarMapLayers;
class RadarLayerSelection;
class MapLayerCache;
class RadarLayerLifecycle;
struct RadarMapPreparationDiagnostic;

struct RadarSinks
{
    SmokeRegistry* smoke{};
    RadarBroadcast* broadcast{};
    const PlayersLayer* players{};
    const RadarMapLayers* map_layers{};
    const RadarLayerSelection* layer_selection{};
    const MapLayerCache* layer_cache{};
    const RadarLayerLifecycle* layer_lifecycle{};
    const RadarMapPreparationDiagnostic* layer_preparation{};
    const std::string* layer_image_waiting_reason{};
    const char* map_name{};
    const char* correction_path{};
    const float* analysis_ms{};
};

void RadarHooks_SetSinks(const RadarSinks& sinks);
void RadarHooks_ClearSinks();
const RadarSinks& RadarHooks_Sinks();

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
