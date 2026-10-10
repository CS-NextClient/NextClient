#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <ncl_math/vec3.h>
#include <goldsrc/fwd.h>

#include "hud/radar/map/MapGeometry.h"

struct BombSite
{
    ncl_math::Vector3 pos;
    std::vector<ncl_math::Aabb3> volumes;
    char label[2];
};

class RadarMapObjectives
{
public:
    // Must precede sites_, whose type uses this array bound.
    static constexpr int kMaxSites = 4;

private:
    struct EntityKeys
    {
        char classname[64];
        char model[64];
        char target[64];
        char targetname[64];
        float origin[3];
        bool has_origin;
    };

    BombSite sites_[kMaxSites]{};
    size_t revision_{};
    int site_count_{};
    int hostage_count_{};
    std::vector<std::string> hostage_models_;
    char loaded_map_[64]{};

public:
    void EnsureLoaded();
    size_t get_revision() const
    {
        return revision_;
    }
    int get_site_count() const
    {
        return site_count_;
    }
    int get_hostage_count() const
    {
        return hostage_count_;
    }
    // The models the map's hostage entities wear, each once.
    std::span<const std::string> get_hostage_models() const
    {
        return hostage_models_;
    }
    const BombSite& get_site(int i) const
    {
        return sites_[i];
    }

private:
    void Parse(goldsrc::model_t* world);

    void AssignSiteLetters();

    // A truncated block still yields its partial keys, unvalidated.
    static char* ParseEntityKeys(char* data, EntityKeys& out);

    static bool BombSiteBounds(goldsrc::model_t* world, const EntityKeys& keys, ncl_math::Aabb3& out);
};
