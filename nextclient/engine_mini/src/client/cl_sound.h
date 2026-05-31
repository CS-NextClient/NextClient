#pragma once
#include <string>
#include <unordered_set>

typedef nitroapi::NextHandlerInterface<void, int, int, sfx_t*, vec_t*, float, float, int, int> S_StartDynamicSoundChain;

void S_StartDynamicSoundHook(
    int entnum,
    int entchannel,
    sfx_t* sfx,
    vec_t* origin,
    float fvol,
    float attenuation,
    int flags,
    int pitch,
    S_StartDynamicSoundChain* next
);

void S_UnloadSounds(const std::unordered_set<std::string>& names);
