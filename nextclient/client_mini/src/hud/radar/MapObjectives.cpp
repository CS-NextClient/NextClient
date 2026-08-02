#include "MapObjectives.h"
#include "RadarUtil.h"
#include "../../main.h"

#include <strtools.h>

#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace
{
    // Per-map A/B override: overviews/radar_ab.txt holds "mapname" "letters"
    // pairs; letters are assigned to sites in left-to-right (world X) order.
    // NextClient-only file, separate from the standard overview .txt.
    bool LoadAbOverride(const char* base_map, char* out, int max_len)
    {
        int length = 0;
        char* buffer = reinterpret_cast<char*>(gEngfuncs.COM_LoadFile(const_cast<char*>("overviews/radar_ab.txt"), 5, &length));

        if (!buffer)
        {
            return false;
        }

        char* data = buffer;
        // COM_ParseFile writes into this buffer up to the engine token size (2048);
        // a smaller buffer overflows the stack on a long token (see Parse below).
        char token[2048];
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
                V_strncpy(out, token, max_len);
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
    model_t* world = gEngfuncs.hudGetModelByIndex(1);

    if (!world || !world->entities || !world->submodels)
    {
        site_count_ = 0;
        hostage_count_ = 0;
        loaded_map_[0] = '\0';
        return;
    }

    if (std::strcmp(world->name, loaded_map_) == 0)
    {
        return;
    }

    V_strcpy_safe(loaded_map_, world->name);

    site_count_ = 0;
    hostage_count_ = 0;
    Parse(world);
}

void RadarMapObjectives::Parse(model_t* world)
{
    char* data = world->entities;
    // COM_ParseFile writes into this buffer; the engine token buffer is 2048,
    // so the destination must match to survive long worldspawn values (e.g. the
    // de_inferno "wad" key) that overflow a smaller buffer and corrupt the stack.
    char token[2048];
    char site_names[kMaxSites][32] = {};

    while ((data = gEngfuncs.COM_ParseFile(data, token)) != nullptr)
    {
        if (token[0] != '{')
        {
            continue;
        }

        char classname[64] = {};
        char model[64] = {};
        char target[64] = {};
        char targetname[64] = {};
        float origin[3] = {};
        bool has_origin = false;

        while (true)
        {
            data = gEngfuncs.COM_ParseFile(data, token);

            if (!data || token[0] == '}')
            {
                break;
            }

            char key[64];
            V_strcpy_safe(key, token);

            data = gEngfuncs.COM_ParseFile(data, token);

            if (!data)
            {
                break;
            }

            if (!std::strcmp(key, "classname"))
            {
                V_strcpy_safe(classname, token);
            }
            else if (!std::strcmp(key, "model"))
            {
                V_strcpy_safe(model, token);
            }
            else if (!std::strcmp(key, "target"))
            {
                V_strcpy_safe(target, token);
            }
            else if (!std::strcmp(key, "targetname"))
            {
                V_strcpy_safe(targetname, token);
            }
            else if (!std::strcmp(key, "origin"))
            {
                char* p = token;
                origin[0] = std::strtof(p, &p);
                origin[1] = std::strtof(p, &p);
                origin[2] = std::strtof(p, &p);
                has_origin = true;
            }
        }

        // Both classnames map to CHostage server-side (hostage.cpp LINK_ENTITY_TO_CLASS).
        if (!std::strcmp(classname, "hostage_entity") || !std::strcmp(classname, "monster_scientist"))
        {
            hostage_count_++;
            continue;
        }

        bool is_site = !std::strcmp(classname, "func_bomb_target") || !std::strcmp(classname, "info_bomb_target");

        if (!is_site || site_count_ >= kMaxSites)
        {
            continue;
        }

        float cx, cy;

        if (model[0] == '*')
        {
            int n = atoi(model + 1);

            if (n < 0 || n >= world->numsubmodels)
            {
                continue;
            }

            dmodel_t& sm = world->submodels[n];
            cx = (sm.mins[0] + sm.maxs[0]) * 0.5f;
            cy = (sm.mins[1] + sm.maxs[1]) * 0.5f;
        }
        else if (has_origin)
        {
            cx = origin[0];
            cy = origin[1];
        }
        else
        {
            continue;
        }

        // Several brushes can make up one bomb site (e.g. de_prodigy target1 x3);
        // merge them by name so a site counts once.
        const char* site_name = target[0] ? target : targetname;

        if (site_name[0] &&
            std::any_of(site_names, site_names + site_count_, [&](const char* n) { return !stricmp(n, site_name); }))
        {
            continue;
        }

        char letter = RadarUtil_DetectSiteLetter(target);

        if (!letter)
        {
            letter = RadarUtil_DetectSiteLetter(targetname);
        }

        V_strcpy_safe(site_names[site_count_], site_name);

        sites_[site_count_].pos = {cx, cy};
        sites_[site_count_].label[0] = letter; // 0 when the map does not name it
        sites_[site_count_].label[1] = '\0';
        site_count_++;
    }

    // 1) explicit per-map override (radar_ab.txt): letters in world-X order.
    char base_map[64];
    RadarUtil_ExtractMapName(loaded_map_, base_map, sizeof(base_map));
    char letters[8] = {};

    if (LoadAbOverride(base_map, letters, sizeof(letters)))
    {
        std::sort(sites_, sites_ + site_count_, [](const BombSite& a, const BombSite& b) { return a.pos.x < b.pos.x; });

        for (int i = 0; i < site_count_ && letters[i]; i++)
        {
            sites_[i].label[0] = letters[i];
            sites_[i].label[1] = '\0';
        }

        return;
    }

    // 2) letters encoded in the BSP target names (e.g. de_dust2 tgt_a/tgt_b).
    bool all_named =
        site_count_ >= 1 && std::all_of(sites_, sites_ + site_count_, [](const BombSite& s) { return s.label[0] != 0; });

    if (all_named)
    {
        return;
    }

    // 3) fallback: stable left-to-right A, B, C, ... by world X.
    std::sort(sites_, sites_ + site_count_, [](const BombSite& a, const BombSite& b) { return a.pos.x < b.pos.x; });

    for (int i = 0; i < site_count_; i++)
    {
        sites_[i].label[0] = (char)('A' + i);
        sites_[i].label[1] = '\0';
    }
}
