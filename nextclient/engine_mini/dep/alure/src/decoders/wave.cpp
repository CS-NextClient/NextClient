
#include "wave.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include "buffer.h"


namespace {

constexpr int FORMAT_TYPE_PCM        = 0x0001;
constexpr int FORMAT_TYPE_FLOAT      = 0x0003;
constexpr int FORMAT_TYPE_MULAW      = 0x0007;
constexpr int FORMAT_TYPE_EXTENSIBLE = 0xFFFE;

struct IDType {
    using ubyte16 = ALubyte[16];
    alure::Array<ALubyte,16> mGuid;
};

inline bool operator==(const IDType::ubyte16 &lhs, const IDType &rhs)
{
    static_assert(sizeof(lhs) == sizeof(rhs.mGuid), "Invalid ID size");
    return std::equal(std::begin(lhs), std::end(lhs), rhs.mGuid.begin());
}

const IDType SUBTYPE_PCM{{{
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa,
    0x00, 0x38, 0x9b, 0x71
}}};
const IDType SUBTYPE_FLOAT{{{
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa,
    0x00, 0x38, 0x9b, 0x71
}}};

const IDType SUBTYPE_BFORMAT_PCM{{{
    0x01, 0x00, 0x00, 0x00, 0x21, 0x07, 0xd3, 0x11, 0x86, 0x44, 0xc8, 0xc1,
    0xca, 0x00, 0x00, 0x00
}}};
const IDType SUBTYPE_BFORMAT_FLOAT{{{
    0x03, 0x00, 0x00, 0x00, 0x21, 0x07, 0xd3, 0x11, 0x86, 0x44, 0xc8, 0xc1,
    0xca, 0x00, 0x00, 0x00
}}};

constexpr int CHANNELS_MONO       = 0x04;
constexpr int CHANNELS_STEREO     = 0x01 | 0x02;
constexpr int CHANNELS_QUAD       = 0x01 | 0x02               | 0x10 | 0x20;
constexpr int CHANNELS_5DOT1      = 0x01 | 0x02 | 0x04 | 0x08                       | 0x200 | 0x400;
constexpr int CHANNELS_5DOT1_REAR = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20;
constexpr int CHANNELS_6DOT1      = 0x01 | 0x02 | 0x04 | 0x08               | 0x100 | 0x200 | 0x400;
constexpr int CHANNELS_7DOT1      = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x200         | 0x400;


ALuint read_le32(std::istream &stream)
{
    char buf[4];
    if(!stream.read(buf, sizeof(buf)) || stream.gcount() != sizeof(buf))
        return 0;
    return ((ALuint(buf[0]    )&0x000000ff) | (ALuint(buf[1]<< 8)&0x0000ff00) |
            (ALuint(buf[2]<<16)&0x00ff0000) | (ALuint(buf[3]<<24)&0xff000000));
}

ALushort read_le16(std::istream &stream)
{
    char buf[2];
    if(!stream.read(buf, sizeof(buf)) || stream.gcount() != sizeof(buf))
        return 0;
    return ((ALushort(buf[0]   )&0x00ff) | (ALushort(buf[1]<<8)&0xff00));
}

} // namespace

namespace alure {

class WaveDecoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    ChannelConfig mChannelConfig{ChannelConfig::Mono};
    SampleType mSampleType{SampleType::UInt8};
    ALuint mFrequency{0};
    ALuint mFrameSize{0};

    // In sample frames, relative to sample data start
    std::pair<uint64_t,uint64_t> mLoopPts{0, 0};
    bool mHasLoopPts{false};

    // In bytes from beginning of file
    std::istream::pos_type mStart{0}, mEnd{0};
    std::istream::pos_type mCurrentPos{0};

public:
    WaveDecoder(UniquePtr<std::istream> file, ChannelConfig channels, SampleType type,
                ALuint frequency, ALuint framesize, std::istream::pos_type start,
                std::istream::pos_type end, uint64_t loopstart, uint64_t loopend, bool hasLoopPts) noexcept
      : mFile(std::move(file)), mChannelConfig(channels), mSampleType(type), mFrequency(frequency)
      , mFrameSize(framesize)
      , mLoopPts{loopstart, loopend}
      , mStart(start)
      , mEnd(end)
      , mHasLoopPts{hasLoopPts}
    { mCurrentPos = mFile->tellg(); }
    ~WaveDecoder() override { }

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    bool hasLoopPoints() const noexcept override;
    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

ALuint WaveDecoder::getFrequency() const noexcept { return mFrequency; }
ChannelConfig WaveDecoder::getChannelConfig() const noexcept { return mChannelConfig; }
SampleType WaveDecoder::getSampleType() const noexcept { return mSampleType; }

uint64_t WaveDecoder::getLength() const noexcept
{ return (mEnd - mStart) / mFrameSize; }

bool WaveDecoder::seek(uint64_t pos) noexcept
{
    std::streamsize offset = pos*mFrameSize + mStart;
    mFile->clear();
    if(offset > mEnd || !mFile->seekg(offset))
        return false;
    mCurrentPos = offset;
    return true;
}

bool WaveDecoder::hasLoopPoints() const noexcept { return mHasLoopPts; }

std::pair<uint64_t,uint64_t> WaveDecoder::getLoopPoints() const noexcept { return mLoopPts; }

ALuint WaveDecoder::read(ALvoid *ptr, ALuint count) noexcept
{
    mFile->clear();

    ALuint total = 0;
    if(mCurrentPos < mEnd)
    {
        ALuint len = static_cast<ALuint>(
            std::min<std::istream::pos_type>(count*mFrameSize, mEnd-mCurrentPos)
        );
#ifdef __BIG_ENDIAN__
        switch(mSampleType)
        {
            case SampleType::Float32:
                while(total < len && mFile->good() && !mFile->eof())
                {
                    char temp[256];
                    ALuint todo = std::min<ALuint>(len-total, sizeof(temp));

                    mFile->read(temp, todo);
                    ALuint got = static_cast<ALuint>(mFile->gcount());

                    for(ALuint i = 0;i < got;++i)
                        reinterpret_cast<char*>(ptr)[total+i] = temp[i^3];

                    mCurrentPos += got;
                    total += got;
                    if(got < todo) break;
                }
                total /= mFrameSize;
                break;

            case SampleType::Int16:
                while(total < len && mFile->good() && !mFile->eof())
                {
                    char temp[256];
                    ALuint todo = std::min<ALuint>(len-total, sizeof(temp));

                    mFile->read(temp, todo);
                    ALuint got = static_cast<ALuint>(mFile->gcount());

                    for(ALuint i = 0;i < got;++i)
                        reinterpret_cast<char*>(ptr)[total+i] = temp[i^1];

                    mCurrentPos += got;
                    total += got;
                    if(got < todo) break;
                }
                total /= mFrameSize;
                break;

            case SampleType::UInt8:
            case SampleType::Mulaw:
#else
        {
#endif
                mFile->read(reinterpret_cast<char*>(ptr), len);
                ALuint got = static_cast<ALuint>(mFile->gcount());

                mCurrentPos += got;
                total = got / mFrameSize;
        }
    }

    return total;
}


SharedPtr<Decoder> WaveDecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    ChannelConfig channels = ChannelConfig::Mono;
    SampleType type = SampleType::UInt8;
    ALuint frequency = 0;
    ALuint framesize = 0;
    uint64_t loop_pts[2]{0, 0};
    ALuint blockalign = 0;
    ALuint framealign = 0;

    char tag_[4]{};
    if(!file->read(tag_, 4) || file->gcount() != 4 || memcmp(tag_, "RIFF", 4) != 0)
        return nullptr;
    ALuint totalsize = read_le32(*file) & ~1u;
    if(!file->read(tag_, 4) || file->gcount() != 4 || memcmp(tag_, "WAVE", 4) != 0)
        return nullptr;

    while(file->good() && !file->eof() && totalsize > 8)
    {
        if(!file->read(tag_, 4) || file->gcount() != 4)
            return nullptr;
        ALuint size = read_le32(*file);
        if(size < 2) return nullptr;
        totalsize -= 8;

        size = std::min(size, totalsize);
        ALuint padbyte = size & 1u;
        totalsize -= size+padbyte;

        StringView tag(tag_, 4);
        if(tag == "fmt ")
        {
            /* 'fmt ' tag needs at least 16 bytes. */
            if(size < 16) goto next_chunk;

            int fmttype = read_le16(*file); size -= 2;
            int chancount = read_le16(*file); size -= 2;
            frequency = read_le32(*file); size -= 4;

            /* skip average bytes per second */
            read_le32(*file); size -= 4;

            blockalign = read_le16(*file); size -= 2;
            int bitdepth = read_le16(*file); size -= 2;

            /* Look for any extra data and try to find the format */
            ALuint extrabytes = 0;
            if(size >= 2)
            {
                extrabytes = read_le16(*file);
                size -= 2;
            }
            extrabytes = std::min<ALuint>(extrabytes, size);

            if(fmttype == FORMAT_TYPE_PCM)
            {
                if(chancount == 1)
                    channels = ChannelConfig::Mono;
                else if(chancount == 2)
                    channels = ChannelConfig::Stereo;
                else
                    goto next_chunk;

                if(bitdepth == 8)
                    type = SampleType::UInt8;
                else if(bitdepth == 16)
                    type = SampleType::Int16;
                else
                    goto next_chunk;
            }
            else if(fmttype == FORMAT_TYPE_FLOAT)
            {
                if(chancount == 1)
                    channels = ChannelConfig::Mono;
                else if(chancount == 2)
                    channels = ChannelConfig::Stereo;
                else
                    goto next_chunk;

                if(bitdepth == 32)
                    type = SampleType::Float32;
                else
                    goto next_chunk;
            }
            else if(fmttype == FORMAT_TYPE_MULAW)
            {
                if(chancount == 1)
                    channels = ChannelConfig::Mono;
                else if(chancount == 2)
                    channels = ChannelConfig::Stereo;
                else
                    goto next_chunk;

                if(bitdepth == 8)
                    type = SampleType::Mulaw;
                else
                    goto next_chunk;
            }
            else if(fmttype == FORMAT_TYPE_EXTENSIBLE)
            {
                if(size < 22) goto next_chunk;

                ALubyte subtype[16];
                ALushort validbits = read_le16(*file); size -= 2;
                ALuint chanmask = read_le32(*file); size -= 4;
                file->read(reinterpret_cast<char*>(subtype), 16);
                size -= static_cast<ALuint>(file->gcount());

                /* Padded bit depths not supported */
                if(validbits != bitdepth)
                    goto next_chunk;

                if(subtype == SUBTYPE_BFORMAT_PCM || subtype == SUBTYPE_BFORMAT_FLOAT)
                {
                    if(chanmask != 0)
                        goto next_chunk;

                    if(chancount == 3)
                        channels = ChannelConfig::BFormat2D;
                    else if(chancount == 4)
                        channels = ChannelConfig::BFormat3D;
                    else
                        goto next_chunk;
                }
                else if(subtype == SUBTYPE_PCM || subtype == SUBTYPE_FLOAT)
                {
                    if(chancount == 1 && chanmask == CHANNELS_MONO)
                        channels = ChannelConfig::Mono;
                    else if(chancount == 2 && chanmask == CHANNELS_STEREO)
                        channels = ChannelConfig::Stereo;
                    else if(chancount == 4 && chanmask == CHANNELS_QUAD)
                        channels = ChannelConfig::Quad;
                    else if(chancount == 6 && (chanmask == CHANNELS_5DOT1 || chanmask == CHANNELS_5DOT1_REAR))
                        channels = ChannelConfig::X51;
                    else if(chancount == 7 && chanmask == CHANNELS_6DOT1)
                        channels = ChannelConfig::X61;
                    else if(chancount == 8 && chanmask == CHANNELS_7DOT1)
                        channels = ChannelConfig::X71;
                    else
                        goto next_chunk;
                }

                if(subtype == SUBTYPE_PCM || subtype == SUBTYPE_BFORMAT_PCM)
                {
                    if(bitdepth == 8)
                        type = SampleType::UInt8;
                    else if(bitdepth == 16)
                        type = SampleType::Int16;
                    else
                        goto next_chunk;
                }
                else if(subtype == SUBTYPE_FLOAT || subtype == SUBTYPE_BFORMAT_FLOAT)
                {
                    if(bitdepth == 32)
                        type = SampleType::Float32;
                    else
                        goto next_chunk;
                }
                else
                    goto next_chunk;
            }
            else
                goto next_chunk;

            framesize = FramesToBytes(1, channels, type);

            /* Calculate the number of frames per block (ADPCM will need extra
             * consideration). */
            framealign = blockalign / framesize;
        }
        else if(tag == "smpl")
        {
            /* sampler data needs at least 36 bytes */
            if(size < 36) goto next_chunk;

            /* Most of this only affects MIDI sampling, but we only care about
             * the loop definitions at the end. */
            /*ALuint manufacturer =*/ read_le32(*file);
            /*ALuint product =*/ read_le32(*file);
            /*ALuint smpperiod =*/ read_le32(*file);
            /*ALuint unitynote =*/ read_le32(*file);
            /*ALuint pitchfrac =*/ read_le32(*file);
            /*ALuint smptefmt =*/ read_le32(*file);
            /*ALuint smpteoffset =*/ read_le32(*file);
            ALuint loopcount = read_le32(*file);
            /*ALuint extrabytes =*/ read_le32(*file);
            size -= 36;

            for(ALuint i = 0;i < loopcount && size >= 24;++i)
            {
                /*ALuint id =*/ read_le32(*file);
                ALuint type = read_le32(*file);
                ALuint loopstart = read_le32(*file);
                ALuint loopend = read_le32(*file);
                /*ALuint frac =*/ read_le32(*file);
                ALuint numloops = read_le32(*file);
                size -= 24;

                /* Only handle indefinite forward loops. */
                if(type == 0 && numloops == 0)
                {
                    loop_pts[0] = loopstart;
                    loop_pts[1] = loopend;
                    break;
                }
            }
        }
        else if(tag == "data")
        {
            if(framesize == 0 || !Context::GetCurrent().isSupported(channels, type))
                goto next_chunk;

            /* Make sure there's at least one sample frame of audio data. */
            std::istream::pos_type start = file->tellg();
            std::istream::pos_type end = start + std::istream::pos_type(size - (size%framesize));
            if(end-start >= framesize)
            {
                /* Loop points are byte offsets relative to the data start.
                 * Convert to sample frame offsets. */
                bool hasLoopPts = false;
                if(loop_pts[0] > 0)
                    hasLoopPts = true;
                return MakeShared<WaveDecoder>(std::move(file),
                    channels, type, frequency, framesize, start, end,
                    loop_pts[0] / blockalign * framealign,
                    loop_pts[1] / blockalign * framealign,
                    hasLoopPts
                );
            }
        }

    next_chunk:
        size += padbyte;
        if(size > 0)
            file->ignore(size);
    }

    return nullptr;
}

} // namespace alure
