#include "MapObjectives.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <span>

#include "main.h"

#include <strtools.h>
#include <goldsrc/client_api.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/format/bsp.h>

#include "hud/radar/RadarLimits.h"
#include "hud/radar/integration/RadarUtil.h"

namespace
{
    // CHostage::Precache gives a hostage this model when its map entity names none.
    constexpr char kHostageDefaultModel[] = "models/scientist.mdl";

    // overviews/radar_ab.txt holds "mapname" "letters" pairs; letters map to sites in world-X order.
    bool LoadAbOverride(const char* base_map, std::span<char> out)
    {
        int length = 0;
        char* buffer = reinterpret_cast<char*>(gEngfuncs.COM_LoadFile(const_cast<char*>("overviews/radar_ab.txt"), 5, &length));

        if (!buffer)
        {
            return false;
        }

        char* data = buffer;
        char token[kComParseTokenSize];
        bool found = false;

        while ((data = gEngfuncs.COM_ParseFile(data, token)) != nullptr)
        {
            char map_key[64];
            V_strcpy_safe(map_key, token);

            data = gEngfuncs.COM_ParseFile(data, token);

            if (!data)
            {
                break;
            }

            if (!stricmp(map_key, base_map))
            {
                V_strncpy(out.data(), token, (int)out.size());
                found = true;
                break;
            }
        }

        gEngfuncs.COM_FreeFile(buffer);
        return found;
    }
} // namespace

void RadarMapObjectives::EnsureLoaded()
{
    goldsrc::model_t* world = gEngfuncs.hudGetModelByIndex(1);

    if (!world || !world->entities || !world->submodels)
    {
        if (loaded_map_[0] || site_count_ || hostage_count_ || !hostage_models_.empty())
        {
            ++revision_;
        }
        std::fill(std::begin(sites_), std::end(sites_), BombSite{});
        site_count_ = 0;
        hostage_count_ = 0;
        hostage_models_.clear();
        loaded_map_[0] = '\0';
        return;
    }

    if (std::strcmp(world->name, loaded_map_) == 0)
    {
        return;
    }

    V_strcpy_safe(loaded_map_, world->name);

    std::fill(std::begin(sites_), std::end(sites_), BombSite{});
    site_count_ = 0;
    hostage_count_ = 0;
    hostage_models_.clear();
    Parse(world);
    ++revision_;
}

char* RadarMapObjectives::ParseEntityKeys(char* data, EntityKeys& out)
{
    char token[kComParseTokenSize];

    out = EntityKeys{};

    while (true)
    {
        data = gEngfuncs.COM_ParseFile(data, token);

        if (!data || token[0] == '}')
        {
            return data;
        }

        char key[64];
        V_strcpy_safe(key, token);

        data = gEngfuncs.COM_ParseFile(data, token);

        if (!data)
        {
            return data;
        }

        if (!std::strcmp(key, "classname"))
        {
            V_strcpy_safe(out.classname, token);
        }
        else if (!std::strcmp(key, "model"))
        {
            V_strcpy_safe(out.model, token);
        }
        else if (!std::strcmp(key, "target"))
        {
            V_strcpy_safe(out.target, token);
        }
        else if (!std::strcmp(key, "targetname"))
        {
            V_strcpy_safe(out.targetname, token);
        }
        else if (!std::strcmp(key, "origin"))
        {
            char* p = token;
            out.origin[0] = std::strtof(p, &p);
            out.origin[1] = std::strtof(p, &p);
            out.origin[2] = std::strtof(p, &p);
            out.has_origin = true;
        }
    }
}

bool RadarMapObjectives::BombSiteBounds(goldsrc::model_t* world, const EntityKeys& keys, ncl_math::Aabb3& out)
{
    if (keys.model[0] == '*')
    {
        int n = atoi(keys.model + 1);

        if (n < 0 || n >= world->numsubmodels)
        {
            return false;
        }

        goldsrc::dmodel_t& sm = world->submodels[n];
        out = {ncl_math::Vector3::Ref(sm.mins), ncl_math::Vector3::Ref(sm.maxs)};

        return true;
    }

    if (keys.has_origin)
    {
        out = {ncl_math::Vector3::Ref(keys.origin), ncl_math::Vector3::Ref(keys.origin)};

        return true;
    }

    return false;
}

void RadarMapObjectives::AssignSiteLetters()
{
    char base_map[64];
    RadarUtil_ExtractMapName(loaded_map_, base_map);
    char letters[8] = {};

    if (LoadAbOverride(base_map, letters))
    {
        std::sort(sites_, sites_ + site_count_, [](const BombSite& a, const BombSite& b) { return a.pos.x < b.pos.x; });

        for (int i = 0; i < site_count_ && letters[i]; i++)
        {
            sites_[i].label[0] = letters[i];
            sites_[i].label[1] = '\0';
        }

        return;
    }

    bool all_named =
        site_count_ >= 1 && std::all_of(sites_, sites_ + site_count_, [](const BombSite& s) { return s.label[0] != 0; });

    if (all_named)
    {
        return;
    }

    std::sort(sites_, sites_ + site_count_, [](const BombSite& a, const BombSite& b) { return a.pos.x < b.pos.x; });

    for (int i = 0; i < site_count_; i++)
    {
        sites_[i].label[0] = (char)('A' + i);
        sites_[i].label[1] = '\0';
    }
}

void RadarMapObjectives::Parse(goldsrc::model_t* world)
{
    char* data = world->entities;
    char token[kComParseTokenSize];
    char site_names[kMaxSites][32] = {};

    while ((data = gEngfuncs.COM_ParseFile(data, token)) != nullptr)
    {
        if (token[0] != '{')
        {
            continue;
        }

        EntityKeys keys;
        data = ParseEntityKeys(data, keys);

        // Both classnames map to CHostage server-side (hostage.cpp LINK_ENTITY_TO_CLASS).
        if (!std::strcmp(keys.classname, "hostage_entity") || !std::strcmp(keys.classname, "monster_scientist"))
        {
            hostage_count_++;

            const char* model = keys.model[0] ? keys.model : kHostageDefaultModel;

            if (std::ranges::find(hostage_models_, model) == hostage_models_.end())
            {
                hostage_models_.emplace_back(model);
            }

            continue;
        }

        bool is_site =
            !std::strcmp(keys.classname, "func_bomb_target") || !std::strcmp(keys.classname, "info_bomb_target");

        if (!is_site)
        {
            continue;
        }

        ncl_math::Aabb3 bounds;

        if (!BombSiteBounds(world, keys, bounds))
        {
            continue;
        }

        // Several brushes can make up one site (de_prodigy target1 x3); merge by name so it counts once.
        const char* site_name = keys.target[0] ? keys.target : keys.targetname;

        bool merged = false;

        if (site_name[0])
        {
            for (int i = 0; i < site_count_; i++)
            {
                if (!stricmp(site_names[i], site_name))
                {
                    sites_[i].volumes.push_back(bounds);
                    merged = true;
                    break;
                }
            }
        }

        if (merged || site_count_ >= kMaxSites)
        {
            continue;
        }

        char letter = RadarUtil_DetectSiteLetter(keys.target);

        if (!letter)
        {
            letter = RadarUtil_DetectSiteLetter(keys.targetname);
        }

        V_strcpy_safe(site_names[site_count_], site_name);

        sites_[site_count_] = BombSite{};
        sites_[site_count_].pos = (bounds.min + bounds.max) * 0.5f;
        sites_[site_count_].volumes.push_back(bounds);
        sites_[site_count_].label[0] = letter; // 0 when the map does not name it
        sites_[site_count_].label[1] = '\0';
        site_count_++;
    }

    AssignSiteLetters();
}
