#pragma once

#include <ncl_math/vec3.h>

typedef struct model_s model_t;

struct BombSite
{
    ncl_math::Vector2 pos;
    char label[2];
};

// Static objectives read from the BSP entity lump: bomb sites (func_bomb_target /
// info_bomb_target) and the hostage count (hostage_entity / monster_scientist). Brush
// targets take their centre from the submodel bounds; point targets use the origin key.
// Reparsed when the level changes.
//
// The counts also tell RadarPolicy what the map can possibly be about: a bomb or
// hostage message on a map holding neither is being used for something else.
class RadarMapObjectives
{
private:
    static constexpr int kMaxSites = 4; // must precede the array below

    BombSite sites_[kMaxSites]{};
    int site_count_ = 0;
    int hostage_count_ = 0;
    char loaded_map_[64]{};

public:
    void EnsureLoaded();
    int site_count() const
    {
        return site_count_;
    }
    int hostage_count() const
    {
        return hostage_count_;
    }
    const BombSite& site(int i) const
    {
        return sites_[i];
    }

private:
    void Parse(model_t* world);
};
