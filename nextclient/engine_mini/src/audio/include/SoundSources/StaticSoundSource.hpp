#pragma once

#include "BaseSoundSource.hpp"


namespace MetaAudio
{
  class StaticSoundSource final : public BaseSoundSource
  {
  private:
    std::chrono::time_point<std::chrono::steady_clock> m_start_time;
    std::chrono::time_point<std::chrono::steady_clock> m_maximum_end_time;

    size_t m_frequency;
    size_t m_length;

    alure::Buffer m_buffer;

    float* al_xfi_workaround;

  public:
    StaticSoundSource(const alure::Buffer& buffer, const alure::Source& source);

    void Play() override;

    bool IsPlaying() override;
  };
}