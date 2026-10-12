#pragma once

#include <array>
#include <cstdint>
#include <span>

inline constexpr size_t kMapLayerImageLimit = 3;

struct MapLayerEntry
{
    int layer = -1;
    uint64_t last_frame{};
    bool valid{};
};

struct MapLayerRequest
{
    int slot = -1;
    bool render{};
};

class MapLayerCache
{
private:
    std::array<MapLayerEntry, kMapLayerImageLimit> entries_{};
    uint64_t generation_frame_{};
    bool has_generation_{};

public:
    MapLayerRequest Request(int layer, uint64_t frame);
    void Commit(int slot, int layer);
    void Reset();

    std::span<const MapLayerEntry> get_entries() const;
};
