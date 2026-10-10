#include "MarkerIdentity.h"

#include <algorithm>

MarkerIdentity::MarkerIdentity(const Params& params) :
    params_(params)
{
}

void MarkerIdentity::Reset()
{
    entries_.fill(Entry{});
}

MarkerIdentity::Entry* MarkerIdentity::AcquireEntry(int index)
{
    for (Entry& entry : entries_)
    {
        if (entry.used && entry.index == index)
        {
            return &entry;
        }
    }

    for (Entry& slot : entries_)
    {
        if (!slot.used)
        {
            slot = Entry{};
            slot.used = true;
            slot.index = index;
            return &slot;
        }
    }

    return nullptr;
}

bool MarkerIdentity::is_ent_paired(int ent) const
{
    if (ent < 0)
    {
        return false;
    }

    return std::ranges::any_of(entries_, [ent](const Entry& entry) { return entry.used && entry.ent == ent; });
}

void MarkerIdentity::RecheckPair(Entry& entry, std::span<const Entity> entities, ncl_math::Vector3 pos)
{
    auto paired = std::ranges::find_if(entities, [&entry](const Entity& entity) { return entity.ent == entry.ent; });

    // Outside the snapshot there is nothing to hold the pair against.
    if (paired == entities.end())
    {
        return;
    }

    if (ncl_math::DistanceSqr(paired->pos, pos) <= params_.match_tolerance * params_.match_tolerance)
    {
        entry.strikes = 0;
        return;
    }

    if (++entry.strikes >= params_.break_strikes)
    {
        entry.ent = -1;
        entry.strikes = 0;
    }
}

void MarkerIdentity::Update(std::span<const Entity> entities, std::span<const Observation> observations)
{
    struct Match
    {
        Entry* entry;
        int ent;
        float dist_sqr;
    };

    Match matches[kRadarMaxMarkers];
    int match_count = 0;

    for (const Observation& observation : observations)
    {
        Entry* entry = AcquireEntry(observation.index);

        if (!entry || (entry->ent >= 0 && entry->seq == observation.seq))
        {
            continue;
        }

        entry->seq = observation.seq;

        if (entry->ent >= 0)
        {
            RecheckPair(*entry, entities, observation.pos);
            continue;
        }

        int best_ent = -1;
        float best_dist_sqr = params_.match_tolerance * params_.match_tolerance;

        for (const Entity& entity : entities)
        {
            if (is_ent_paired(entity.ent))
            {
                continue;
            }

            float dist_sqr = ncl_math::DistanceSqr(entity.pos, observation.pos);

            if (dist_sqr <= best_dist_sqr)
            {
                best_dist_sqr = dist_sqr;
                best_ent = entity.ent;
            }
        }

        if (best_ent >= 0 && match_count < kRadarMaxMarkers)
        {
            matches[match_count++] = {entry, best_ent, best_dist_sqr};
        }
    }

    // Nearest first, one entity per index: a batch of messages cannot pile onto one entity.
    std::sort(matches, matches + match_count, [](const Match& a, const Match& b) { return a.dist_sqr < b.dist_sqr; });

    for (int i = 0; i < match_count; i++)
    {
        if (matches[i].entry->ent >= 0 || is_ent_paired(matches[i].ent))
        {
            continue;
        }

        matches[i].entry->ent = matches[i].ent;
        matches[i].entry->strikes = 0;
    }
}

int MarkerIdentity::get_entity(int index) const
{
    for (const Entry& entry : entries_)
    {
        if (entry.used && entry.index == index)
        {
            return entry.ent;
        }
    }

    return -1;
}

int MarkerIdentity::get_index(int ent) const
{
    if (ent < 0)
    {
        return -1;
    }

    for (const Entry& entry : entries_)
    {
        if (entry.used && entry.ent == ent)
        {
            return entry.index;
        }
    }

    return -1;
}
