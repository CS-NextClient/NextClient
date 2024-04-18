#ifndef DECODERS_FLAC_HPP
#define DECODERS_FLAC_HPP

#include "alure2.h"

namespace alure {

class FlacDecoderFactory final : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) noexcept override;
};

} // namespace alure

#endif /* DECODERS_FLAC_HPP */
