#pragma once

#include <vector>
#include <string>
#include "snd_local.h"

void AUDIO_Init();
void AUDIO_Shutdown();
void AUDIO_RegisterCommands();

void S_UnloadSounds(const std::vector<std::string>& names);
sfxcache_t* S_LoadSound(sfx_t* sound, channel_t* channel);
void VoiceSE_NotifyFreeChannel(int channel);
sentenceEntry_s* SequenceGetSentenceByIndex(unsigned int index);