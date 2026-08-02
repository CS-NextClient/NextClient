#pragma once

#include <ncl_math/vec3.h>

typedef struct model_s model_t;

struct BombSite
{
    ncl_math::Vector2 pos;
    char label[2];
};

class RadarMapObjectives
{
    static constexpr int kMaxSites = 4;

    BombSite sites_[kMaxSites]{};
    int site_count_{};
    int hostage_count_{};
    char loaded_map_[64]{};

public:
    void EnsureLoaded();
    int get_site_count() const
    {
        return site_count_;
    }
    int get_hostage_count() const
    {
        return hostage_count_;
    }
    const BombSite& get_site(int i) const
    {
        return sites_[i];
    }

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

    void Parse(model_t* world);

    // A truncated block still yields its partial keys, unvalidated.
    static char* ParseEntityKeys(char* data, EntityKeys& out);

    static bool BombSiteCenter(model_t* world, const EntityKeys& keys, ncl_math::Vector2& out);

    void AssignSiteLetters();
};
