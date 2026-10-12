#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "hud/radar/map/MapGeometry.h"
#include "hud/radar/map/MapReachability.h"
#include "hud/radar/map/layers/MapLayerOverrides.h"

namespace taskcoro
{
    class CancellationToken;
}

enum class RadarMapStatus
{
    Waiting,
    Ready,
    Failed,
};

enum class RadarLayerQuery
{
    Below,
    Nearest,
};

struct RadarLayerInfo
{
    int id{};
    std::string name;
    float height{};
    float area{};
};

struct RadarLayerMatch
{
    int layer = -1;
    ncl_math::Vector3 support{};
    bool contained{};
    int surface_id = -1;
};

struct RadarLayerGroupAnalysis
{
    int layer{};
    float area{};
    ncl_math::Aabb2 bounds = ncl_math::Aabb2::Empty();
    float min_height{};
    float max_height{};
    std::vector<std::string> reasons;
};

struct RadarLayerTransition
{
    int lower{};
    int upper{};
    float height{};
    std::vector<int> surface_ids;
    float height_span{};
};

struct RadarLayerAnalysis
{
    RadarMapStatus status = RadarMapStatus::Waiting;
    std::vector<RadarLayerGroupAnalysis> layers;
    std::vector<RadarLayerTransition> transitions;
    std::vector<std::string> manual_conflicts;
    float overlap_area_threshold{};
    float largest_overlap_area{};
    size_t significant_overlap_regions{};
    RadarMapReachability reachability;
};

struct RadarLayerSupport
{
    RadarMapSurface surface;
    int layer = -1;
    int automatic_layer = -1;
};

struct RadarSupportNode
{
    ncl_math::Aabb3 bounds{};
    size_t first{};
    size_t count{};
    int children[2] = {-1, -1};
    std::vector<std::pair<int, ncl_math::Aabb3>> layer_bounds;
};

struct RadarLayerLayout
{
    std::vector<RadarLayerSupport> supports;
    std::vector<RadarLayerInfo> layers;
    RadarLayerAnalysis analysis;
};

struct RadarAutomaticLayout
{
    RadarMapGeometry geometry;
    RadarLayerLayout layout;
};

class RadarMapLayers
{
private:
    std::shared_ptr<const RadarAutomaticLayout> automatic_;
    std::optional<RadarLayerLayout> corrected_layout_;
    std::vector<ncl_math::Aabb3> support_bounds_;
    std::vector<size_t> nearest_ids_;
    std::vector<RadarSupportNode> nearest_nodes_;
    std::map<std::pair<int, int>, std::vector<size_t>> index_;

    std::vector<std::vector<RadarMapSurface>> meshes_;
    ncl_math::Aabb2 bounds_{};

    RadarLayerOverrides overrides_;
    std::vector<int> override_ids_;

public:
    RadarMapStatus Build(RadarMapGeometry geometry, const taskcoro::CancellationToken* token = nullptr);
    RadarMapStatus Build(
        RadarMapGeometry geometry,
        const RadarLayerOverrides& overrides,
        std::string& warning,
        const taskcoro::CancellationToken* token = nullptr
    );
    bool ApplyOverrides(const RadarLayerOverrides& overrides, std::string& error, const taskcoro::CancellationToken* token = nullptr);
    void Reset();

    std::span<const RadarLayerInfo> get_layers() const;
    const ncl_math::Aabb2& get_bounds() const;
    std::span<const RadarMapSurface> get_surfaces(int layer) const;

    RadarLayerMatch Locate(ncl_math::Vector3 position, RadarLayerQuery query, int preferred = -1, int layer = -1) const;
    std::vector<int> Intersect(ncl_math::Aabb3 volume) const;
    void Intersect(ncl_math::Aabb3 volume, std::vector<int>& layers) const;

    const RadarLayerAnalysis& get_analysis() const;

private:
    const RadarLayerLayout& get_layout() const;
    RadarMapStatus BuildAutomatic(RadarMapGeometry geometry, const taskcoro::CancellationToken* token);
    void Rebuild(const taskcoro::CancellationToken* token);
    bool RebuildBounds(const taskcoro::CancellationToken* token);
    RadarLayerMatch LocateSupport(ncl_math::Vector3 position, RadarLayerQuery query, int preferred, int layer, bool below_plane) const;
    int BuildLookup(size_t first, size_t count, const taskcoro::CancellationToken* token);
};

struct RadarLayerView
{
    const RadarMapLayers* map{};
    int active = -1;
    ncl_math::Vector3 origin{};
    bool enabled{};
    std::optional<float> active_support_z;
};

struct RadarLayerObservation
{
    ncl_math::Vector3 feet{};
    std::optional<ncl_math::Vector3> support;
    float time{};
    int source{};
    bool valid{};
    bool grounded{};
    bool free_camera{};
    bool discontinuity{};
};

enum class RadarLayerReason
{
    Initial,
    Unchanged,
    Pending,
    Confirmed,
    Grounded,
    Airborne,
    Discontinuity,
    Invalid,
    Nearest,
};

class RadarLayerSelection
{
private:
    RadarLayerObservation previous_{};
    bool has_previous_{};
    bool reselect_{};
    int active_ = -1;
    int candidate_ = -1;
    float candidate_since_{};
    RadarLayerReason reason_ = RadarLayerReason::Initial;

public:
    int Update(const RadarMapLayers& map, const RadarLayerObservation& observation);
    void Reset();

    int get_active() const;
    int get_candidate() const;
    float get_candidate_since() const;
    RadarLayerReason get_reason() const;
    int get_source() const;

private:
    int Hold(RadarLayerReason reason);
    int Select(int layer, RadarLayerReason reason);
};
