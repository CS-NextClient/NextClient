
#include "sndfile.hpp"

#include <stdexcept>
#include <iostream>

#include "sndfile.h"

namespace {

constexpr alure::Array<int,1> CHANNELS_MONO      {{SF_CHANNEL_MAP_MONO}};
constexpr alure::Array<int,2> CHANNELS_STEREO    {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT}};
constexpr alure::Array<int,2> CHANNELS_REAR      {{SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT}};
constexpr alure::Array<int,4> CHANNELS_QUAD      {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT}};
constexpr alure::Array<int,6> CHANNELS_5DOT1     {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_SIDE_LEFT, SF_CHANNEL_MAP_SIDE_RIGHT}};
constexpr alure::Array<int,6> CHANNELS_5DOT1_REAR{{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT}};
constexpr alure::Array<int,7> CHANNELS_6DOT1     {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_REAR_CENTER, SF_CHANNEL_MAP_SIDE_LEFT, SF_CHANNEL_MAP_SIDE_RIGHT}};
constexpr alure::Array<int,8> CHANNELS_7DOT1     {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT, SF_CHANNEL_MAP_SIDE_LEFT, SF_CHANNEL_MAP_SIDE_RIGHT}};
constexpr alure::Array<int,3> CHANNELS_BFORMAT2D {{SF_CHANNEL_MAP_AMBISONIC_B_W, SF_CHANNEL_MAP_AMBISONIC_B_X, SF_CHANNEL_MAP_AMBISONIC_B_Y}};
constexpr alure::Array<int,4> CHANNELS_BFORMAT3D {{SF_CHANNEL_MAP_AMBISONIC_B_W, SF_CHANNEL_MAP_AMBISONIC_B_X, SF_CHANNEL_MAP_AMBISONIC_B_Y, SF_CHANNEL_MAP_AMBISONIC_B_Z}};


sf_count_t istream_get_filelen(void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);
    file->clear();

    sf_count_t len = -1;
    std::streampos pos = file->tellg();
    if(pos != static_cast<std::streampos>(-1) && file->seekg(0, std::ios::end))
    {
        len = file->tellg();
        file->seekg(pos);
    }
    return len;
}

sf_count_t istream_seek(sf_count_t offset, int whence, void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);
    file->clear();

    if(!file->seekg(offset, std::ios::seekdir(whence)))
        return -1;
    return file->tellg();
}

sf_count_t istream_read(void *ptr, sf_count_t count, void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);
    file->clear();

    file->read(reinterpret_cast<char*>(ptr), count);
    return file->gcount();
}

sf_count_t istream_write(const void*, sf_count_t, void*)
{
    return -1;
}

sf_count_t istream_tell(void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);
    file->clear();

    return file->tellg();
}


struct SndfileDeleter {
    void operator()(SNDFILE *ptr) const { sf_close(ptr); }
};
using SndfilePtr = alure::UniquePtr<SNDFILE,SndfileDeleter>;

} // namespace

namespace alure {

class SndFileDecoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    SndfilePtr mSndFile;
    SF_INFO mSndInfo;

    ChannelConfig mChannelConfig{ChannelConfig::Mono};
    SampleType mSampleType{SampleType::UInt8};
    bool mHasLoopPts{false};
    std::pair<uint64_t, uint64_t> mLoopPts{0, 0};

public:
    SndFileDecoder(UniquePtr<std::istream> file, SndfilePtr sndfile, const SF_INFO &sndinfo,
                   ChannelConfig sconfig, SampleType stype, uint64_t loopstart, uint64_t loopend,
                   bool hasloop) noexcept
      : mFile(std::move(file)), mSndFile(std::move(sndfile)), mSndInfo(sndinfo)
      , mChannelConfig(sconfig), mSampleType(stype), mLoopPts{loopstart, loopend}
      , mHasLoopPts{hasloop}
    { }
    ~SndFileDecoder() override { }

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    bool hasLoopPoints() const noexcept override;
    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

ALuint SndFileDecoder::getFrequency() const noexcept { return mSndInfo.samplerate; }
ChannelConfig SndFileDecoder::getChannelConfig() const noexcept { return mChannelConfig; }
SampleType SndFileDecoder::getSampleType() const noexcept { return mSampleType; }

uint64_t SndFileDecoder::getLength() const noexcept
{
    return std::max<sf_count_t>(mSndInfo.frames, 0);
}

bool SndFileDecoder::seek(uint64_t pos) noexcept
{
    sf_count_t newpos = sf_seek(mSndFile.get(), pos, SEEK_SET);
    if(newpos < 0) return false;
    return true;
}

bool SndFileDecoder::hasLoopPoints() const noexcept { return mHasLoopPts; }

std::pair<uint64_t, uint64_t> SndFileDecoder::getLoopPoints() const noexcept { return mLoopPts; }

ALuint SndFileDecoder::read(ALvoid *ptr, ALuint count) noexcept
{
    sf_count_t got = 0;
    switch (mSampleType)
    {
        case SampleType::Mulaw:
        case SampleType::UInt8:
            got = BytesToFrames(sf_read_raw(mSndFile.get(), static_cast<ALubyte *>(ptr),
                                FramesToBytes(count, mChannelConfig, mSampleType)),
                                mChannelConfig, mSampleType);
            break;
        case SampleType::Int16:
            got = sf_readf_short(mSndFile.get(), static_cast<short *>(ptr), count);
            break;
        case SampleType::Float32:
            got = sf_readf_float(mSndFile.get(), static_cast<float *>(ptr), count);
            break;
    }
    return (ALuint)std::max<sf_count_t>(got, 0);
}


SharedPtr<Decoder> SndFileDecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    SF_VIRTUAL_IO vio = {
        istream_get_filelen, istream_seek,
        istream_read, istream_write, istream_tell
    };
    SF_INFO sndinfo;
    SndfilePtr sndfile(sf_open_virtual(&vio, SFM_READ, &sndinfo, file.get()));
    if(!sndfile) return nullptr;

    bool has_cues = false;
    std::pair<uint64_t, uint64_t> cue_points{0, std::numeric_limits<uint64_t>::max()};
    {
        // Needed for compatibility with older sndfile libraries.
        struct SNDFILE_CUE_POINT {
            int32_t indx;
            uint32_t position;
            int32_t fcc_chunk;
            int32_t chunk_start;
            int32_t block_start;
            uint32_t sample_offset;
            char name[256];
        };

        struct {
            uint32_t cue_count;
            SNDFILE_CUE_POINT cue_points[100];
        } cues;

        enum { SNDFILE_GET_CUE = 0x10CE };

        if(sf_command(sndfile.get(), SNDFILE_GET_CUE, &cues, sizeof(cues)))
        {
            has_cues = true;
            cue_points.first = cues.cue_points[0].sample_offset;
            if(cues.cue_count > 1)
            {
                cue_points.second = cues.cue_points[1].sample_offset;
            }
        }
    }

    ChannelConfig sconfig;
    Vector<int> chanmap(sndinfo.channels);
    if(sf_command(sndfile.get(), SFC_GET_CHANNEL_MAP_INFO, chanmap.data(), chanmap.size()*sizeof(int)) == SF_TRUE)
    {
        auto matches = [](const Vector<int> &first, ArrayView<int> second) -> bool
        {
            return (first.size() == second.size()) &&
                   std::equal(first.begin(), first.end(), second.begin());
        };

        if(matches(chanmap, CHANNELS_MONO))
            sconfig = ChannelConfig::Mono;
        else if(matches(chanmap, CHANNELS_STEREO))
            sconfig = ChannelConfig::Stereo;
        else if(matches(chanmap, CHANNELS_REAR))
            sconfig = ChannelConfig::Rear;
        else if(matches(chanmap, CHANNELS_QUAD))
            sconfig = ChannelConfig::Quad;
        else if(matches(chanmap, CHANNELS_5DOT1) || matches(chanmap, CHANNELS_5DOT1_REAR))
            sconfig = ChannelConfig::X51;
        else if(matches(chanmap, CHANNELS_6DOT1))
            sconfig = ChannelConfig::X61;
        else if(matches(chanmap, CHANNELS_7DOT1))
            sconfig = ChannelConfig::X71;
        else if(matches(chanmap, CHANNELS_BFORMAT2D))
            sconfig = ChannelConfig::BFormat2D;
        else if(matches(chanmap, CHANNELS_BFORMAT3D))
            sconfig = ChannelConfig::BFormat3D;
        else
            return nullptr;
    }
    else if(sf_command(sndfile.get(), SFC_WAVEX_GET_AMBISONIC, nullptr, 0) == SF_AMBISONIC_B_FORMAT)
    {
        if(sndinfo.channels == 3)
            sconfig = ChannelConfig::BFormat2D;
        else if(sndinfo.channels == 4)
            sconfig = ChannelConfig::BFormat3D;
        else
            return nullptr;
    }
    else if(sndinfo.channels == 1)
        sconfig = ChannelConfig::Mono;
    else if(sndinfo.channels == 2)
        sconfig = ChannelConfig::Stereo;
    else
        return nullptr;

    SampleType stype = SampleType::Int16;
    switch(sndinfo.format&SF_FORMAT_SUBMASK)
    {
        case SF_FORMAT_PCM_U8:
            stype = SampleType::UInt8;
            break;
        case SF_FORMAT_ULAW:
            if(Context::GetCurrent().isSupported(sconfig, SampleType::Mulaw))
                stype = SampleType::Mulaw;
            break;
        case SF_FORMAT_FLOAT:
        case SF_FORMAT_DOUBLE:
        case SF_FORMAT_VORBIS:
            if(Context::GetCurrent().isSupported(sconfig, SampleType::Float32))
                stype = SampleType::Float32;
            break;
        default:
            // For everything else, decode to signed 16-bit
            stype = SampleType::Int16;
            break;
    }

    return MakeShared<SndFileDecoder>(std::move(file), std::move(sndfile), sndinfo, sconfig, stype,
        cue_points.first, cue_points.second, has_cues);
}

} // namespace alure
