#ifndef DECODERS_VORBISFILE_HPP
#define DECODERS_VORBISFILE_HPP

#include "alure2.h"

namespace alure {

class VorbisFileDecoderFactory final : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) noexcept override;
};

} // namespace alure

#endif /* DECODERS_VORBISFILE_HPP */
