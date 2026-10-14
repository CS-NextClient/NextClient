#include "MapLayerCache.h"

#include <algorithm>
#include <cassert>

MapLayerRequest MapLayerCache::Request(int layer, uint64_t frame)
{
    for (size_t i = 0; i < entries_.size(); i++)
    {
        MapLayerEntry& entry = entries_[i];

        if (entry.valid && entry.layer == layer)
        {
            entry.last_frame = frame;

            return {(int)i, false};
        }
    }

    if (layer < 0 || (has_generation_ && generation_frame_ == frame))
    {
        return {};
    }

    has_generation_ = true;
    generation_frame_ = frame;

    auto slot = std::find_if(entries_.begin(), entries_.end(), [](const MapLayerEntry& entry) { return !entry.valid; });

    if (slot == entries_.end())
    {
        slot = std::min_element(entries_.begin(), entries_.end(), [](const MapLayerEntry& a, const MapLayerEntry& b) {
            return a.last_frame < b.last_frame;
        });
    }

    *slot = {-1, frame, false};

    return {(int)(slot - entries_.begin()), true};
}

void MapLayerCache::Commit(int slot, int layer)
{
    assert(slot >= 0 && (size_t)slot < entries_.size() && layer >= 0);

    entries_[slot].layer = layer;
    entries_[slot].valid = true;
}

void MapLayerCache::Reset()
{
    entries_.fill(MapLayerEntry{});
    generation_frame_ = 0;
    has_generation_ = false;
}

std::span<const MapLayerEntry> MapLayerCache::get_entries() const
{
    return entries_;
}
