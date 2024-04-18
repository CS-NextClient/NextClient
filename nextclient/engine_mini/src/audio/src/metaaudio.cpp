#include <optick.h>

#include "snd_local.h"
#include "Utilities/AudioCache.hpp"
#include "Loaders/SoundLoader.hpp"
#include "Vox/VoxManager.hpp"
#include "Config/SettingsManager.hpp"
#include "AudioEngine.hpp"
#include "../../engine.h"

MetaAudio::SettingsManager settings;
static std::shared_ptr<MetaAudio::SoundLoader> sound_loader;
static std::unique_ptr<MetaAudio::AudioEngine> audio_engine;

static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsubs;

static void AL_Version() { audio_engine->AL_Version(); }
static void AL_ResetEFX() { audio_engine->AL_ResetEFX(); }
static void AL_BasicDevices() { audio_engine->AL_Devices(true); }
static void AL_FullDevices() { audio_engine->AL_Devices(false); }

void AUDIO_Init()
{
    if (COM_CheckParm("-nosound"))
        return;

    auto audio_cache = std::make_shared<MetaAudio::AudioCache>();
    sound_loader = std::make_shared<MetaAudio::SoundLoader>(audio_cache);
    audio_engine = std::make_unique<MetaAudio::AudioEngine>(audio_cache, sound_loader);

    g_Unsubs.emplace_back(eng()->SNDDMA_Init |= [](const auto& next) {
        int result = next->Invoke();
        audio_engine->SNDDMA_Init();
        return result;
    });

    g_Unsubs.emplace_back(eng()->S_Init |= [](const auto& next) {
        next->Invoke();
        audio_engine->S_Init();
    });

    g_Unsubs.emplace_back(eng()->S_Shutdown |= [](const auto& next) {
        audio_engine->S_Shutdown();
        next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->S_FindName |= [](const char* name, int* pfInCache, const auto& next) {
        return audio_engine->S_FindName(name, pfInCache);
    });

    g_Unsubs.emplace_back(eng()->S_StartDynamicSound |= [](int entnum, int entchannel, sfx_t* sfx, float* origin, float fvol, float attenuation, int flags, int pitch, const auto& next) {
        audio_engine->S_StartDynamicSound(entnum, entchannel, sfx, origin, fvol, attenuation, flags, pitch);
    });

    g_Unsubs.emplace_back(eng()->S_StartStaticSound |= [](int entnum, int entchannel, sfx_t* sfx, float* origin, float fvol, float attenuation, int flags, int pitch, const auto& next) {
        audio_engine->S_StartStaticSound(entnum, entchannel, sfx, origin, fvol, attenuation, flags, pitch);
    });

    g_Unsubs.emplace_back(eng()->S_StopSound |= [](int entnum, int entchannel, const auto& next) {
        audio_engine->S_StopSound(entnum, entchannel);
    });

    g_Unsubs.emplace_back(eng()->S_StopAllSounds |= [](qboolean clear, const auto& next) {
        audio_engine->S_StopAllSounds(clear);
    });

    g_Unsubs.emplace_back(eng()->S_Update |= [](float* origin, float* forward, float* right, float* up, const auto& next) {
        audio_engine->S_Update(origin, forward, right, up);
    });

    g_Unsubs.emplace_back(eng()->S_LoadSound |= [](sfx_t* sound, channel_t* channel, const auto& next) {
        return (sfxcache_t*)sound_loader->S_LoadSound(sound, (aud_channel_t*)channel);
    });
}

void AUDIO_Shutdown()
{
    sound_loader.reset();
    audio_engine.reset();

    for (auto &unsubscriber : g_Unsubs)
        unsubscriber->Unsubscribe();
    g_Unsubs.clear();
}

void AUDIO_RegisterCommands()
{
    if (COM_CheckParm("-nosound"))
        return;

    gEngfuncs.pfnAddCommand("al_version", AL_Version);
    gEngfuncs.pfnAddCommand("al_reset_efx", AL_ResetEFX);
    gEngfuncs.pfnAddCommand("al_show_basic_devices", AL_BasicDevices);
    gEngfuncs.pfnAddCommand("al_show_full_devices", AL_FullDevices);
}

sfx_t* S_PrecacheSound(char *sample)
{
    OPTICK_EVENT();

    return eng()->S_PrecacheSound.InvokeChained(sample);
}

void S_UnloadSounds(const std::vector<std::string>& names)
{
    // TODO implement this
    //sound_loader->S_UnloadSounds(names);
}

sfxcache_t* S_LoadSound(sfx_t* sound, channel_t* channel)
{
    return eng()->S_LoadSound(sound, channel);
}

void VoiceSE_NotifyFreeChannel(int channel)
{
    eng()->VoiceSE_NotifyFreeChannel(channel);
}

sentenceEntry_s* SequenceGetSentenceByIndex(unsigned int index)
{
    return eng()->SequenceGetSentenceByIndex(index);
}