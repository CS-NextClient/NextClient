#ifndef DECODERS_MP3_HPP
#define DECODERS_MP3_HPP

#include "alure2.h"

namespace alure {

class Mp3DecoderFactory final : public DecoderFactory {
public:
    Mp3DecoderFactory() noexcept;
    ~Mp3DecoderFactory() override;

    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) noexcept override;
};

} // namespace alure

#endif /* DECODERS_MP3_HPP */

