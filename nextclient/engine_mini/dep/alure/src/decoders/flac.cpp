
#include "flac.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include "main.h"

#define DR_FLAC_NO_STDIO
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"


namespace {

struct FlacFileDeleter {
    void operator()(drflac *ptr) const { drflac_close(ptr); }
};
using FlacFilePtr = alure::UniquePtr<drflac,FlacFileDeleter>;

}


namespace alure {

class FlacDecoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    FlacFilePtr mFlacFile;
    ChannelConfig mChannelConfig{ChannelConfig::Mono};
    SampleType mSampleType{SampleType::UInt8};
    ALuint mFrequency{0};
    bool mHasLoopPts {false};
    std::pair<uint64_t,uint64_t> mLoopPts{0, 0};

    static void MetadataCallback(void *client_data, drflac_metadata *mdata)
    {
        FlacDecoder *self = static_cast<FlacDecoder*>(client_data);

        if(mdata->type == DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO)
        {
            // Ignore duplicate StreamInfo blocks
            if(self->mFrequency != 0)
                return;

            const drflac_streaminfo &info = mdata->data.streaminfo;
            if(info.channels == 1)
                self->mChannelConfig = ChannelConfig::Mono;
            else if(info.channels == 2)
                self->mChannelConfig = ChannelConfig::Stereo;
            else
                return;

            if(info.bitsPerSample > 16 &&
               Context::GetCurrent().isSupported(self->mChannelConfig, SampleType::Float32))
                self->mSampleType = SampleType::Float32;
            else
                self->mSampleType = SampleType::Int16;

            self->mFrequency = info.sampleRate;
        }
        else if(mdata->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
        {
            const auto &vc = mdata->data.vorbis_comment;
            drflac_vorbis_comment_iterator iter;
            drflac_uint32 comment_len;
            const char *comment_str;

            drflac_init_vorbis_comment_iterator(&iter, vc.commentCount, vc.pComments);
            while((comment_str=drflac_next_vorbis_comment(&iter, &comment_len)) != nullptr)
            {
                auto seppos = StringView(comment_str, comment_len).find_first_of('=');
                if(seppos == StringView::npos) continue;

                StringView key(comment_str, seppos);
                StringView val(comment_str+seppos+1, comment_len-(seppos+1));

                // RPG Maker seems to recognize LOOPSTART and LOOPLENGTH for
                // loop points in a Vorbis comment. ZDoom recognizes LOOP_START
                // and LOOP_END. We can recognize both.
                if(key == "LOOP_START" || key == "LOOPSTART")
                {
                    self->mHasLoopPts = true;
                    auto pt = ParseTimeval(val, self->mFrequency);
                    if(pt.index() == 1) self->mLoopPts.first = std::get<1>(pt);
                    continue;
                }

                if(key == "LOOP_END")
                {
                    self->mHasLoopPts = true;
                    auto pt = ParseTimeval(val, self->mFrequency);
                    if(pt.index() == 1) self->mLoopPts.second = std::get<1>(pt);
                    continue;
                }

                if(key == "LOOPLENGTH")
                {
                    auto pt = ParseTimeval(val, self->mFrequency);
                    if(pt.index() == 1)
                        self->mLoopPts.second = self->mLoopPts.first + std::get<1>(pt);
                    continue;
                }
            }
        }
    }

    static size_t ReadCallback(void *client_data, void *buffer, size_t bytes)
    {
        std::istream *stream = static_cast<FlacDecoder*>(client_data)->mFile.get();
        stream->clear();

        stream->read(reinterpret_cast<char*>(buffer), bytes);
        return stream->gcount();
    }
    static drflac_bool32 SeekCallback(void *client_data, int offset, drflac_seek_origin origin)
    {
        std::istream *stream = static_cast<FlacDecoder*>(client_data)->mFile.get();
        stream->clear();

        if(!stream->seekg(offset, (origin==drflac_seek_origin_current) ? std::ios_base::cur
                                                                       : std::ios_base::beg))
            return DRFLAC_FALSE;
        return DRFLAC_TRUE;
    }

public:
    FlacDecoder() noexcept { }
    ~FlacDecoder() override { }

    bool open(UniquePtr<std::istream> &file) noexcept;

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    std::pair<uint64_t, uint64_t> getLoopPoints() const noexcept override;

    bool hasLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};


bool FlacDecoder::open(UniquePtr<std::istream> &file) noexcept
{
    mFile = std::move(file);
    mFlacFile = FlacFilePtr(drflac_open_with_metadata(ReadCallback, SeekCallback, MetadataCallback, this));
    if(mFlacFile)
    {
        if(mFrequency != 0)
            return true;

        mFlacFile = nullptr;
    }

    file = std::move(mFile);
    return false;
}


ALuint FlacDecoder::getFrequency() const noexcept
{
    return mFrequency;
}

ChannelConfig FlacDecoder::getChannelConfig() const noexcept
{
    return mChannelConfig;
}

SampleType FlacDecoder::getSampleType() const noexcept
{
    return mSampleType;
}


uint64_t FlacDecoder::getLength() const noexcept
{
    // For some silly reason, dr_flac tracks sample counts as individual
    // samples rather than sample frames (e.g. doubled for stereo).
    return mFlacFile->totalSampleCount / mFlacFile->channels;
}

bool FlacDecoder::seek(uint64_t pos) noexcept
{
    pos *= mFlacFile->channels;
    if(pos >= mFlacFile->totalSampleCount)
        return false;
    return drflac_seek_to_sample(mFlacFile.get(), pos);
}

std::pair<uint64_t,uint64_t> FlacDecoder::getLoopPoints() const noexcept
{
    return mLoopPts;
}

bool FlacDecoder::hasLoopPoints() const noexcept
{
    return mHasLoopPts;
}

ALuint FlacDecoder::read(ALvoid *ptr, ALuint count) noexcept
{
    count *= mFlacFile->channels;
    if(mSampleType == SampleType::Float32)
        count = drflac_read_f32(mFlacFile.get(), count, (float*)ptr);
    else if(mSampleType == SampleType::Int16)
        count = drflac_read_s16(mFlacFile.get(), count, (short*)ptr);
    else
        count = 0;
    return count / mFlacFile->channels;
}


SharedPtr<Decoder> FlacDecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    auto decoder = MakeShared<FlacDecoder>();
    if(!decoder->open(file)) decoder.reset();
    return decoder;
}

}
