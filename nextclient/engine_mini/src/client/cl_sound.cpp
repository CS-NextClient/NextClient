#include "../engine.h"
#include "../common/zone.h"
#include "../common/com_strings.h"
#include "cl_sound.h"
#include <ncl_perf/profiler.h>
#include "ncl_entity/PlayerSyncSystem.h"
#include "ncl_entity/WeaponSyncSystem.h"
#include "ncl_entity/cl_ncl_entity_sync.h"

#include <format>
#include <iterator>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/sound.h>
#include <goldsrc/limits.h>
#include <goldsrc/types.h>

namespace
{
    goldsrc::sfx_t* GetReplacementSfx(int entnum, int entchannel, goldsrc::sfx_t* sfx)
    {
        if (entchannel != goldsrc::CHAN_WEAPON && entchannel != goldsrc::CHAN_ITEM)
        {
            return nullptr;
        }

        PlayerSyncSystem* player_sync =
            static_cast<PlayerSyncSystem*>(CL_NclEntitySyncGetHandler(static_cast<uint8_t>(ncl_entity::EntityTypeId::Player)));

        WeaponSyncSystem* weapon_sync =
            static_cast<WeaponSyncSystem*>(CL_NclEntitySyncGetHandler(static_cast<uint8_t>(ncl_entity::EntityTypeId::Weapon)));

        if (player_sync == nullptr || weapon_sync == nullptr)
        {
            return nullptr;
        }

        if (cls == nullptr || cl == nullptr || cls->state != goldsrc::ca_active)
        {
            return nullptr;
        }

        if (entnum < 1 || entnum > goldsrc::MAX_CLIENTS)
        {
            return nullptr;
        }

        int active_weapon_index = player_sync->GetPlayerActiveWeapon(entnum);
        if (active_weapon_index == 0)
        {
            return nullptr;
        }

        const auto& overrides = weapon_sync->GetOverrides(active_weapon_index);

        auto replace_it = overrides.find(sfx);
        if (replace_it == overrides.end())
        {
            return nullptr;
        }

        return replace_it->second;
    }
} // namespace

goldsrc::sfx_t* S_PrecacheSound(char* sample)
{
    NCL_PROFILE_SCOPE();

    return eng()->S_PrecacheSound(sample);
}

void S_StartDynamicSoundHook(
    int entnum,
    int entchannel,
    goldsrc::sfx_t* sfx,
    goldsrc::vec_t* origin,
    float fvol,
    float attenuation,
    int flags,
    int pitch,
    S_StartDynamicSoundChain* next
)
{
    NCL_PROFILE_SCOPE();

    goldsrc::sfx_t* replacement_sfx = GetReplacementSfx(entnum, entchannel, sfx);
    if (replacement_sfx)
    {
        next->Invoke(entnum, entchannel, replacement_sfx, origin, fvol, attenuation, flags, pitch);
        return;
    }

    next->Invoke(entnum, entchannel, sfx, origin, fvol, attenuation, flags, pitch);
}

void S_StopAllSounds(goldsrc::qboolean clear)
{
    NCL_PROFILE_SCOPE();

    eng()->S_StopAllSounds.InvokeChained(clear);
}

void S_UnloadSounds(const std::unordered_set<std::string>& names)
{
    NCL_PROFILE_SCOPE();

    goldsrc::sfx_t* known_sfx = *p_known_sfx;
    int num_sfx = *p_num_sfx;

    std::string buf;

    for (int i = 0; i < num_sfx; i++)
    {
        goldsrc::sfx_t* sfx = &known_sfx[i];

        buf.clear();
        std::format_to(std::back_inserter(buf), "{}{}", DEFAULT_SOUNDPATH, sfx->name);

        if (!names.contains(buf))
        {
            continue;
        }

        if (Cache_Check(&sfx->cache))
        {
            Cache_Free(&sfx->cache);
        }

        sfx->cache.data = nullptr;
        sfx->name[0] = '\0';
    }
}
