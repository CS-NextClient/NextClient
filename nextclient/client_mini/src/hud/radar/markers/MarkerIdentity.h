#pragma once

#include <array>
#include <cstdint>
#include <span>

#include <ncl_math/vec3.h>

#include "hud/radar/RadarLimits.h"

// Pairs the index a server message carries with the entity the local snapshot carries, for feeds
// whose two id spaces have nothing in common: HostagePos hands out CHostage's spawn counter, not an
// edict, and no entity state carries it. The pair is recovered from the positions, since a message
// and the snapshot it arrives with hold the same origin.
class MarkerIdentity
{
public:
    struct Params
    {
        float match_tolerance = 48.0f; // world units allowed between a message and its entity
        int break_strikes = 3; // messages in a row that must land away before a pair drops
    };

    struct Entity
    {
        int ent;
        ncl_math::Vector3 pos;
        bool discontinuity{};
    };

    struct Observation
    {
        int index;
        ncl_math::Vector3 pos;
        std::uint32_t seq; // a value the feed bumps per message, so a repeat still reads as new
        ncl_math::Vector3 previous_pos{};
        float interval{}; // seconds between the last two received reports; zero without a pair
    };

private:
    struct Entry
    {
        bool used{};
        int index{};
        int ent = -1;
        std::uint32_t seq{}; // last observation accounted for
        int strikes{};
    };

private:
    Params params_;
    std::array<Entry, kRadarMaxMarkers> entries_{};

public:
    explicit MarkerIdentity(const Params& params);

    void Update(std::span<const Entity> entities, std::span<const Observation> observations);

    // -1 when nothing is paired.
    int get_entity(int index) const;
    int get_index(int ent) const;

    // Pairs hold for the level: neither id changes while the map runs.
    void Reset();

private:
    Entry* AcquireEntry(int index);

    bool is_ent_paired(int ent) const;

    // Drops the pair once break_strikes observations in a row land away from the paired entity.
    void RecheckPair(Entry& entry, std::span<const Entity> entities, ncl_math::Vector3 pos);
};
