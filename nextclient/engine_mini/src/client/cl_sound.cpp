#include "../engine.h"
#include "../common/zone.h"
#include "../common/com_strings.h"
#include "optick.h"

sfx_t* S_PrecacheSound(char* sample)
{
    OPTICK_EVENT();

    return eng()->S_PrecacheSound(sample);
}

void S_UnloadSounds(const std::unordered_set<std::string>& names)
{
    OPTICK_EVENT();

    sfx_t* known_sfx = *p_known_sfx;
    int num_sfx = *p_num_sfx;

    int i;
    sfx_t* sfx;

    for (i = 0; i < num_sfx; i++)
    {
        sfx = &known_sfx[i];

        if (!names.contains(std::format("{}{}", DEFAULT_SOUNDPATH, sfx->name)))
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
