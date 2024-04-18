#pragma once
#include <queue>

#include "sound_internal.h"

//internal structures

struct GainFading
{
  float current{ 0 };
  float elapsed_time{ 0 };
  float initial_value{ 0 };
  float last_target{ 0 };
  float target{ 0 };
};

namespace MetaAudio
{
  class AudioEngine;
  class SoundLoader;
  class VoxManager;
  class BaseSoundSource;
}

struct aud_channel_t
{
  sfx_t* sfx{ nullptr };
  float volume{ 1.0f };
  float pitch{ 1.0f };
  float attenuation{ 0 };
  int entnum{ 0 };
  int entchannel{ 0 };
  vec3_t origin{ 0.0f, 0.0f, 0.0f };
  uint64_t start{ 0 };
  uint64_t end{ 0 };
  //for sentence
  std::queue<voxword_t> words;
  //for voice sound
  sfxcache_t* voicecache{ nullptr };

  // For OpenAL
  alure::SharedPtr<MetaAudio::BaseSoundSource> sound_source;

  MetaAudio::VoxManager* vox{ nullptr };

  // For OpenAL EFX
  bool firstpass{ true };
  GainFading LowGain;
  GainFading MidGain;
  GainFading HighGain;

  aud_channel_t() = default;
  ~aud_channel_t();

  aud_channel_t(const aud_channel_t& other) = delete;
  aud_channel_t& operator=(const aud_channel_t& other) = delete;

  aud_channel_t(aud_channel_t&& other) = default;
  aud_channel_t& operator=(aud_channel_t&& other) = default;
};

struct aud_sfxcache_t
{
  //wave info
  uint64_t length{};
  uint64_t loopstart{};
  uint64_t loopend{};
  ALuint samplerate{};
  bool looping{};
  bool force_streaming{};
  alure::SampleType stype{};
  alure::ChannelConfig channels{};

  //OpenAL buffer
  alure::SharedPtr<alure::Decoder> decoder;
  alure::Buffer buffer;

  alure::Vector<ALubyte> data;
};
