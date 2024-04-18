#pragma once
#include <optional>

#include "alure2.h"
#include "LocalAudioDecoder.hpp"
#include "../Utilities/AudioCache.hpp"

namespace MetaAudio
{
  class SoundLoader final
  {
  private:
    std::shared_ptr<AudioCache> m_cache;
    std::shared_ptr<LocalAudioDecoder> m_decoder;

    // Check if file exists. Order: original, .wav, .flac, .ogg, .mp3
    std::optional<alure::String> S_GetFilePath(const alure::String& sfx_name, bool is_stream);
    aud_sfxcache_t* S_LoadStreamSound(sfx_t* s, aud_channel_t* ch);
  public:
    SoundLoader(const std::shared_ptr<AudioCache>& cache);
    aud_sfxcache_t* S_LoadSound(sfx_t* s, aud_channel_t* ch);
  };
}