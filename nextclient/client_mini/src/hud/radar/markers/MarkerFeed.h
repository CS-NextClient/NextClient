#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <ncl_math/vec3.h>

#include "hud/radar/markers/MarkerIdentity.h"
#include "hud/radar/RadarLimits.h"

// The marker set of one radar feature, resolved from the two feeds such a feature can have: the
// entities the local snapshot carries and the positions the server broadcasts for everything else.
// A thing both feeds carry is reported once, from the snapshot, and a broadcast position is played
// back over the interval its messages arrive at, so a marker outside the PVS walks instead of
// stepping. Source transitions preserve the last drawn position; returning to the snapshot
// blends out the position difference before tracking it directly.
class MarkerFeed
{
public:
    struct Params
    {
        MarkerIdentity::Params identity;

        // Set when the message already names the entity, as the Radar message does with the
        // player's own index, which leaves nothing to pair.
        bool index_is_entity{};

        // Bounds on the send interval the playback is timed by.
        float min_interval = 0.2f; // seconds
        float max_interval = 1.5f; // seconds

        // Further than the thing can travel in one interval, so a respawn or a teleport jumps
        // instead of sweeping across the map.
        float snap_dist = 600.0f; // world units
        float fallback_speed = 250.0f; // world units per second without a usable report pair
    };

    struct Marker
    {
        ncl_math::Vector3 pos;
        int ent; // -1 when only the broadcast carries it
        int index; // -1 when only the snapshot carries it
    };

private:
    // Broadcast playback and position correction when its entity returns to the snapshot.
    struct Motion
    {
        bool used;
        int index;
        std::uint32_t seq;
        ncl_math::Vector3 from;
        ncl_math::Vector3 to;
        float start; // client time the segment began
        float duration; // seconds, the interval the previous two reports were apart
        ncl_math::Vector3 shown;
        bool snapshot{};
        ncl_math::Vector3 snapshot_pos;
        ncl_math::Vector3 snapshot_offset;
        float snapshot_time{};
        float snapshot_speed{};
    };

    Params params_;
    MarkerIdentity identity_;

    std::array<Motion, kRadarMaxMarkers> motions_{};

    // Both feeds at once, since a broadcast position the snapshot does not cover adds a marker.
    std::array<Marker, kRadarMaxMarkers * 2> markers_{};
    int marker_count_{};
    float last_time_{};

public:
    explicit MarkerFeed(const Params& params);

    void Update(
        std::span<const MarkerIdentity::Entity> entities,
        std::span<const MarkerIdentity::Observation> observations,
        float time
    );

    std::span<const Marker> get_markers() const
    {
        return {markers_.data(), (size_t)marker_count_};
    }

    // -1 when nothing is paired.
    int get_entity(int index) const;
    int get_index(int ent) const;

    // Drops the playback, so the next report places its marker instead of walking it there.
    void ResetMotion();
    void ResetMotion(int index);

    // Drops the pairs as well; the level is their lifetime.
    void Reset();

private:
    // nullptr when every slot is taken.
    Motion* AcquireMotion(int index, ncl_math::Vector3 pos, std::uint32_t seq, float time);

    float ComputeCorrectionSpeed(const MarkerIdentity::Observation& report) const;
    ncl_math::Vector3 UpdateSnapshot(ncl_math::Vector3 pos, const MarkerIdentity::Observation& report, float time);
    ncl_math::Vector3 Interpolate(int index, ncl_math::Vector3 pos, std::uint32_t seq, float time);
};
