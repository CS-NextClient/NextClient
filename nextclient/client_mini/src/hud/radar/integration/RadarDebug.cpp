#include "RadarDebug.h"

#include "main.h"

#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>

#include "hud/radar/integration/RadarLayersDump.h"
#include "hud/radar/integration/RadarHooks.h"
#include "hud/radar/layers/PlayersLayer.h"

namespace
{
    constexpr size_t kConsoleChunkSize = 512;
    goldsrc::cvar_t* g_Enabled{};
    bool g_LayersCommandRegistered{};

    void DumpMarkers()
    {
        const PlayersLayer* players = RadarHooks_Sinks().players;
        if (players)
        {
            players->DumpMarkers();
        }
    }

    void DumpLayers()
    {
        const RadarSinks& sinks = RadarHooks_Sinks();
        std::string dump = RadarLayersDump_Format(
            sinks.map_layers,
            sinks.layer_selection,
            sinks.layer_cache,
            sinks.map_name ? sinks.map_name : "",
            sinks.correction_path ? sinks.correction_path : "",
            sinks.analysis_ms ? *sinks.analysis_ms : 0.0f,
            sinks.layer_lifecycle,
            sinks.layer_preparation,
            sinks.layer_image_waiting_reason
        );
        size_t start = 0;

        while (start < dump.size())
        {
            std::string chunk = dump.substr(start, kConsoleChunkSize);
            gEngfuncs.Con_Printf("%s", chunk.c_str());
            start += chunk.size();
        }
    }

} // namespace

void RadarDebug_Init()
{
    g_Enabled = gEngfuncs.pfnRegisterVariable("hud_radar_debug", "0", 0);

    if (!g_LayersCommandRegistered)
    {
        gEngfuncs.pfnAddCommand("radar_layers_dump", DumpLayers);
        gEngfuncs.pfnAddCommand("radar_markers_dump", DumpMarkers);
        g_LayersCommandRegistered = true;
    }
}

bool RadarDebug_IsEnabled()
{
    return g_Enabled != nullptr && g_Enabled->value != 0.0f;
}
