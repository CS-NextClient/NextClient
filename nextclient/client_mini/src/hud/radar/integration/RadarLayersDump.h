#pragma once

#include <string>
#include <string_view>

class RadarMapLayers;
class RadarLayerSelection;
class MapLayerCache;
class RadarLayerLifecycle;
struct RadarMapPreparationDiagnostic;

std::string RadarLayersDump_Format(
    const RadarMapLayers* map,
    const RadarLayerSelection* selection,
    const MapLayerCache* cache,
    std::string_view map_name,
    std::string_view correction_path,
    float analysis_ms,
    const RadarLayerLifecycle* lifecycle = nullptr,
    const RadarMapPreparationDiagnostic* preparation = nullptr,
    const std::string* image_waiting_reason = nullptr
);
