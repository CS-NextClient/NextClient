
#include "vorbisfile.hpp"

#include <iostream>

#include "context.h"

#include "vorbis/vorbisfile.h"

namespace {

int istream_seek(void *user_data, ogg_int64_t offset, int whence)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    if(whence == SEEK_CUR)
        stream->seekg(offset, std::ios_base::cur);
    else if(whence == SEEK_SET)
        stream->seekg(offset, std::ios_base::beg);
    else if(whence == SEEK_END)
        stream->seekg(offset, std::ios_base::end);
    else
        return -1;

    return stream->tellg();
}

size_t istream_read(void *ptr, size_t size, size_t nmemb, void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    stream->read(static_cast<char*>(ptr), nmemb*size);
    size_t ret = stream->gcount();
    return ret/size;
}

long istream_tell(void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();
    return stream->tellg();
}

int istream_close(void*) { return 0; }


struct OggVorbisfileHolder : public OggVorbis_File {
    OggVorbisfileHolder() { this->datasource = nullptr; }
    ~OggVorbisfileHolder() { if(this->datasource) ov_clear(this); }
};
using OggVorbisfilePtr = alure::UniquePtr<OggVorbisfileHolder>;

} // namespace

namespace alure {

class VorbisFileDecoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    OggVorbisfilePtr mOggFile;
    vorbis_info *mVorbisInfo{nullptr};
    int mOggBitstream{0};

    ChannelConfig mChannelConfig{ChannelConfig::Mono};

    std::pair<uint64_t,uint64_t> mLoopPoints{0, 0};

public:
    VorbisFileDecoder(UniquePtr<std::istream> file, OggVorbisfilePtr oggfile,
                      vorbis_info *vorbisinfo, ChannelConfig sconfig,
                      std::pair<uint64_t,uint64_t> loop_points) noexcept
      : mFile(std::move(file)), mOggFile(std::move(oggfile)), mVorbisInfo(vorbisinfo)
      , mChannelConfig(sconfig), mLoopPoints(loop_points)
    { }
    ~VorbisFileDecoder() override { }

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

ALuint VorbisFileDecoder::getFrequency() const noexcept { return mVorbisInfo->rate; }
ChannelConfig VorbisFileDecoder::getChannelConfig() const noexcept { return mChannelConfig; }
SampleType VorbisFileDecoder::getSampleType() const noexcept { return SampleType::Int16; }

uint64_t VorbisFileDecoder::getLength() const noexcept
{
    ogg_int64_t len = ov_pcm_total(mOggFile.get(), -1);
    return std::max<ogg_int64_t>(len, 0);
}

bool VorbisFileDecoder::seek(uint64_t pos) noexcept
{
    return ov_pcm_seek(mOggFile.get(), pos) == 0;
}

std::pair<uint64_t,uint64_t> VorbisFileDecoder::getLoopPoints() const noexcept
{
    return mLoopPoints;
}

ALuint VorbisFileDecoder::read(ALvoid *ptr, ALuint count) noexcept
{
    ALuint total = 0;
    ALshort *samples = (ALshort*)ptr;
    while(total < count)
    {
        int len = (count-total) * mVorbisInfo->channels * 2;
#ifdef __BIG_ENDIAN__
        long got = ov_read(mOggFile.get(), reinterpret_cast<char*>(samples), len, 1, 2, 1, &mOggBitstream);
#else
        long got = ov_read(mOggFile.get(), reinterpret_cast<char*>(samples), len, 0, 2, 1, &mOggBitstream);
#endif
        if(got <= 0) break;

        got /= 2;
        samples += got;
        got /= mVorbisInfo->channels;
        total += got;
    }

    // 1, 2, and 4 channel files decode into the same channel order as
    // OpenAL, however 6 (5.1), 7 (6.1), and 8 (7.1) channel files need to be
    // re-ordered.
    if(mChannelConfig == ChannelConfig::X51)
    {
        samples = (ALshort*)ptr;
        for(ALuint i = 0;i < total;++i)
        {
            // OpenAL : FL, FR, FC, LFE, RL, RR
            // Vorbis : FL, FC, FR,  RL, RR, LFE
            std::swap(samples[i*6 + 1], samples[i*6 + 2]);
            std::swap(samples[i*6 + 3], samples[i*6 + 5]);
            std::swap(samples[i*6 + 4], samples[i*6 + 5]);
        }
    }
    else if(mChannelConfig == ChannelConfig::X61)
    {
        samples = (ALshort*)ptr;
        for(ALuint i = 0;i < total;++i)
        {
            // OpenAL : FL, FR, FC, LFE, RC, SL, SR
            // Vorbis : FL, FC, FR,  SL, SR, RC, LFE
            std::swap(samples[i*7 + 1], samples[i*7 + 2]);
            std::swap(samples[i*7 + 3], samples[i*7 + 6]);
            std::swap(samples[i*7 + 4], samples[i*7 + 5]);
            std::swap(samples[i*7 + 5], samples[i*7 + 6]);
        }
    }
    else if(mChannelConfig == ChannelConfig::X71)
    {
        samples = (ALshort*)ptr;
        for(ALuint i = 0;i < total;++i)
        {
            // OpenAL : FL, FR, FC, LFE, RL, RR, SL, SR
            // Vorbis : FL, FC, FR,  SL, SR, RL, RR, LFE
            std::swap(samples[i*8 + 1], samples[i*8 + 2]);
            std::swap(samples[i*8 + 3], samples[i*8 + 7]);
            std::swap(samples[i*8 + 4], samples[i*8 + 5]);
            std::swap(samples[i*8 + 5], samples[i*8 + 6]);
            std::swap(samples[i*8 + 6], samples[i*8 + 7]);
        }
    }

    return total;
}


SharedPtr<Decoder> VorbisFileDecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    static const ov_callbacks streamIO = {
        istream_read, istream_seek, istream_close, istream_tell
    };

    auto oggfile = MakeUnique<OggVorbisfilePtr::element_type>();
    if(ov_open_callbacks(file.get(), oggfile.get(), NULL, 0, streamIO) != 0)
        return nullptr;

    vorbis_info *vorbisinfo = ov_info(oggfile.get(), -1);
    if(!vorbisinfo) return nullptr;

    std::pair<uint64_t,uint64_t> loop_points = { 0, std::numeric_limits<uint64_t>::max() };
    if(vorbis_comment *vc = ov_comment(oggfile.get(), -1))
    {
        for(int i = 0;i < vc->comments;i++)
        {
            StringView val(vc->user_comments[i], vc->comment_lengths[i]);
            auto seppos = val.find_first_of('=');
            if(seppos == StringView::npos) continue;

            StringView key = val.substr(0, seppos);
            val = val.substr(seppos+1);

            // RPG Maker seems to recognize LOOPSTART and LOOPLENGTH for loop
            // points in a Vorbis comment. ZDoom recognizes LOOP_START and
            // LOOP_END. We can recognize both.
            if(key == "LOOP_START" || key == "LOOPSTART")
            {
                auto pt = ParseTimeval(val, vorbisinfo->rate);
                if(pt.index() == 1) loop_points.first = std::get<1>(pt);
                continue;
            }

            if(key == "LOOP_END")
            {
                auto pt = ParseTimeval(val, vorbisinfo->rate);
                if(pt.index() == 1) loop_points.second = std::get<1>(pt);
                continue;
            }

            if(key == "LOOPLENGTH")
            {
                auto pt = ParseTimeval(val, vorbisinfo->rate);
                if(pt.index() == 1)
                    loop_points.second = loop_points.first + std::get<1>(pt);
                continue;
            }
        }
    }

    ChannelConfig channels = ChannelConfig::Mono;
    if(vorbisinfo->channels == 1)
        channels = ChannelConfig::Mono;
    else if(vorbisinfo->channels == 2)
        channels = ChannelConfig::Stereo;
    else if(vorbisinfo->channels == 4)
        channels = ChannelConfig::Quad;
    else if(vorbisinfo->channels == 6)
        channels = ChannelConfig::X51;
    else if(vorbisinfo->channels == 7)
        channels = ChannelConfig::X61;
    else if(vorbisinfo->channels == 8)
        channels = ChannelConfig::X71;
    else
        return nullptr;

    return MakeShared<VorbisFileDecoder>(
        std::move(file), std::move(oggfile), vorbisinfo, channels, loop_points
    );
}

} // namespace alure
