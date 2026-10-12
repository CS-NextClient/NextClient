#pragma once
#include <string>
#include <unordered_set>
#include <goldsrc/engine/sound.h>
#include <goldsrc/types.h>

typedef nitroapi::NextHandlerInterface<void, int, int, goldsrc::sfx_t*, goldsrc::vec_t*, float, float, int, int> S_StartDynamicSoundChain;

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
);

void S_UnloadSounds(const std::unordered_set<std::string>& names);

goldsrc::sfx_t* S_PrecacheSound(char* sample);
void S_StopAllSounds(goldsrc::qboolean clear);
