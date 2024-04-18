#ifndef DECODERS_WAVE_HPP
#define DECODERS_WAVE_HPP

#include "alure2.h"

namespace alure {

class WaveDecoderFactory final : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) noexcept override;
};

} // namespace alure

#endif /* DECODERS_WAVE_HPP */
