#include "RadarLayersDump.h"

#include <iomanip>
#include <locale>
#include <sstream>

#include "hud/radar/map/layers/MapLayerCache.h"
#include "hud/radar/map/MapBspGeometry.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

namespace
{
    const char* Status(RadarMapStatus status)
    {
        switch (status)
        {
            case RadarMapStatus::Waiting:
                return "waiting";
            case RadarMapStatus::Ready:
                return "ready";
            case RadarMapStatus::Failed:
                return "failed";
        }

        return "unknown";
    }

    const char* Reason(RadarLayerReason reason)
    {
        switch (reason)
        {
            case RadarLayerReason::Initial:
                return "initial";
            case RadarLayerReason::Unchanged:
                return "unchanged";
            case RadarLayerReason::Pending:
                return "pending";
            case RadarLayerReason::Confirmed:
                return "confirmed";
            case RadarLayerReason::Grounded:
                return "grounded";
            case RadarLayerReason::Airborne:
                return "airborne";
            case RadarLayerReason::Discontinuity:
                return "discontinuity";
            case RadarLayerReason::Invalid:
                return "invalid";
            case RadarLayerReason::Nearest:
                return "nearest";
        }

        return "unknown";
    }
} // namespace

std::string RadarLayersDump_Format(
    const RadarMapLayers* map,
    const RadarLayerSelection* selection,
    const MapLayerCache* cache,
    std::string_view map_name,
    std::string_view correction_path,
    float analysis_ms,
    const RadarLayerLifecycle* lifecycle,
    const RadarMapPreparationDiagnostic* preparation,
    const std::string* image_waiting_reason
)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(3);

    output << "map=" << map_name << " correction_path=" << correction_path << " analysis_ms=" << analysis_ms << '\n';
    output << "map_state=" << (map ? Status(map->get_analysis().status) : "unavailable") << '\n';

    if (lifecycle)
    {
        output << "geometry_state=" << Status(lifecycle->get_geometry_status()) << " image_state=" << Status(lifecycle->get_image_status())
               << " fallback=" << lifecycle->should_use_fallback()
               << " failure_reason=" << std::quoted(std::string(lifecycle->get_failure_reason())) << '\n';
    }
    else
    {
        output << "geometry_state=unavailable image_state=unavailable fallback=unavailable failure_reason=unavailable\n";
    }

    if (preparation)
    {
        output << "preparation_attempts=" << preparation->attempts << " preparation_mode=" << preparation->mode
               << " capture_ms=" << preparation->capture_ms << " waiting_reason=" << std::quoted(preparation->waiting_reason) << '\n';
    }
    else
    {
        output << "preparation_attempts=unavailable preparation_mode=unavailable capture_ms=unavailable waiting_reason=unavailable\n";
    }

    if (image_waiting_reason)
    {
        output << "image_waiting_reason=" << std::quoted(*image_waiting_reason) << '\n';
    }
    else
    {
        output << "image_waiting_reason=unavailable\n";
    }

    if (map)
    {
        const ncl_math::Aabb2& bounds = map->get_bounds();

        output << "bounds=" << bounds.min.x << ',' << bounds.min.y << ':' << bounds.max.x << ',' << bounds.max.y
               << " layers=" << map->get_layers().size() << '\n';

        const RadarMapReachability& reachability = map->get_analysis().reachability;
        output << "reachability=" << (reachability.available ? "ready" : "unavailable") << " collision_regions=" << reachability.regions
               << " connected_regions=" << reachability.connected_regions << " valid_entry_points=" << reachability.valid_entry_points
               << " excluded_surfaces=" << reachability.excluded_surfaces.size()
               << " reachability_reason=" << std::quoted(reachability.reason) << '\n';

        for (const RadarLayerInfo& layer : map->get_layers())
        {
            output << "layer=" << layer.id << " name=" << std::quoted(layer.name) << " height=" << layer.height << " area=" << layer.area
                   << " mesh_surfaces=" << map->get_surfaces(layer.id).size();

            for (const RadarLayerGroupAnalysis& group : map->get_analysis().layers)
            {
                if (group.layer != layer.id)
                {
                    continue;
                }

                output << " heights=" << group.min_height << ':' << group.max_height << " bounds=" << group.bounds.min.x << ','
                       << group.bounds.min.y << ':' << group.bounds.max.x << ',' << group.bounds.max.y;

                for (const std::string& reason : group.reasons)
                {
                    output << " assignment_reason=" << std::quoted(reason);
                }
            }

            output << '\n';
        }

        output << "overlap_area_threshold=" << map->get_analysis().overlap_area_threshold
               << " largest_overlap_area=" << map->get_analysis().largest_overlap_area
               << " significant_overlap_regions=" << map->get_analysis().significant_overlap_regions << '\n';

        output << "transitions=" << map->get_analysis().transitions.size() << '\n';

        for (const RadarLayerTransition& transition : map->get_analysis().transitions)
        {
            output << "transition=" << transition.lower << ':' << transition.upper << " height=" << transition.height << " surfaces=";

            for (int surface : transition.surface_ids)
            {
                output << surface << ',';
            }

            output << '\n';
        }

        output << "manual_conflicts=" << map->get_analysis().manual_conflicts.size() << '\n';

        for (const std::string& conflict : map->get_analysis().manual_conflicts)
        {
            output << "manual_conflict=" << std::quoted(conflict) << '\n';
        }
    }

    if (selection)
    {
        output << "active_layer=" << selection->get_active() << " candidate_layer=" << selection->get_candidate()
               << " candidate_since=" << selection->get_candidate_since() << " source=" << selection->get_source()
               << " selection_reason=" << Reason(selection->get_reason()) << '\n';
    }
    else
    {
        output << "active_layer=unavailable candidate_layer=unavailable source=unavailable selection_reason=unavailable\n";
    }

    if (cache)
    {
        output << "cache=" << cache->get_entries().size() << '\n';

        size_t slot = 0;

        for (const MapLayerEntry& entry : cache->get_entries())
        {
            output << "cache_slot=" << slot++ << " layer=" << entry.layer << " valid=" << entry.valid << " last_frame=" << entry.last_frame
                   << '\n';
        }
    }
    else
    {
        output << "cache=unavailable\n";
    }

    return output.str();
}
