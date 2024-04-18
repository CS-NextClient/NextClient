
#include "config.h"

#include "buffer.h"

#include <stdexcept>
#include <cstring>

#include "context.h"

namespace {

using alure::Array;
using alure::ArrayView;
using alure::ChannelConfig;
using alure::SampleType;
using alure::AL;

struct FormatListEntry {
    ChannelConfig mChannels;
    AL mExt;
    char mName[32];
};
// NOTE: ChannelConfig values must be present in an ascending order within a given array.
constexpr Array<FormatListEntry,9> UInt8Formats{{
    { ChannelConfig::Mono, AL::EXTENSION_MAX, "AL_FORMAT_MONO8" },
    { ChannelConfig::Stereo, AL::EXTENSION_MAX, "AL_FORMAT_STEREO8" },
    { ChannelConfig::Rear, AL::EXT_MCFORMATS, "AL_FORMAT_REAR8" },
    { ChannelConfig::Quad, AL::EXT_MCFORMATS, "AL_FORMAT_QUAD8" },
    { ChannelConfig::X51, AL::EXT_MCFORMATS, "AL_FORMAT_51CHN8" },
    { ChannelConfig::X61, AL::EXT_MCFORMATS, "AL_FORMAT_61CHN8" },
    { ChannelConfig::X71, AL::EXT_MCFORMATS, "AL_FORMAT_71CHN8" },
    { ChannelConfig::BFormat2D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT2D_8" },
    { ChannelConfig::BFormat3D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT3D_8" }
}}, Int16Formats{{
    { ChannelConfig::Mono, AL::EXTENSION_MAX, "AL_FORMAT_MONO16" },
    { ChannelConfig::Stereo, AL::EXTENSION_MAX, "AL_FORMAT_STEREO16" },
    { ChannelConfig::Rear, AL::EXT_MCFORMATS, "AL_FORMAT_REAR16" },
    { ChannelConfig::Quad, AL::EXT_MCFORMATS, "AL_FORMAT_QUAD16" },
    { ChannelConfig::X51, AL::EXT_MCFORMATS, "AL_FORMAT_51CHN16" },
    { ChannelConfig::X61, AL::EXT_MCFORMATS, "AL_FORMAT_61CHN16" },
    { ChannelConfig::X71, AL::EXT_MCFORMATS, "AL_FORMAT_71CHN16" },
    { ChannelConfig::BFormat2D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT2D_16" },
    { ChannelConfig::BFormat3D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT3D_16" }
}}, FloatFormats{{
    { ChannelConfig::Mono, AL::EXTENSION_MAX, "AL_FORMAT_MONO_FLOAT32" },
    { ChannelConfig::Stereo, AL::EXTENSION_MAX, "AL_FORMAT_STEREO_FLOAT32" },
    { ChannelConfig::Rear, AL::EXT_MCFORMATS, "AL_FORMAT_REAR32" },
    { ChannelConfig::Quad, AL::EXT_MCFORMATS, "AL_FORMAT_QUAD32" },
    { ChannelConfig::X51, AL::EXT_MCFORMATS, "AL_FORMAT_51CHN32" },
    { ChannelConfig::X61, AL::EXT_MCFORMATS, "AL_FORMAT_61CHN32" },
    { ChannelConfig::X71, AL::EXT_MCFORMATS, "AL_FORMAT_71CHN32" },
    { ChannelConfig::BFormat2D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT2D_FLOAT32" },
    { ChannelConfig::BFormat3D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT3D_FLOAT32" }
}}, MulawFormats{{
    { ChannelConfig::Mono, AL::EXTENSION_MAX, "AL_FORMAT_MONO_MULAW" },
    { ChannelConfig::Stereo, AL::EXTENSION_MAX, "AL_FORMAT_STEREO_MULAW" },
    { ChannelConfig::Rear, AL::EXT_MCFORMATS, "AL_FORMAT_REAR_MULAW" },
    { ChannelConfig::Quad, AL::EXT_MCFORMATS, "AL_FORMAT_QUAD_MULAW" },
    { ChannelConfig::X51, AL::EXT_MCFORMATS, "AL_FORMAT_51CHN_MULAW" },
    { ChannelConfig::X61, AL::EXT_MCFORMATS, "AL_FORMAT_61CHN_MULAW" },
    { ChannelConfig::X71, AL::EXT_MCFORMATS, "AL_FORMAT_71CHN_MULAW" },
    { ChannelConfig::BFormat2D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT2D_MULAW" },
    { ChannelConfig::BFormat3D, AL::EXT_BFORMAT, "AL_FORMAT_BFORMAT3D_MULAW" }
}};

const struct {
    SampleType mType;
    AL mExt;
    ArrayView<FormatListEntry> mFormats;
} FormatLists[] = {
    // NOTE: SampleType values must be present in an ascending order within the array.
    { SampleType::UInt8, AL::EXTENSION_MAX, UInt8Formats },
    { SampleType::Int16, AL::EXTENSION_MAX, Int16Formats },
    { SampleType::Float32, AL::EXT_FLOAT32, FloatFormats },
    { SampleType::Mulaw, AL::EXT_MULAW, MulawFormats },
};

} // namespace

namespace alure {

void BufferImpl::cleanup()
{
    alGetError();
    while(!mSources.empty())
    {
        Vector<Source> sources;
        sources.swap(mSources);

        Vector<ALuint> sourceids;
        sourceids.reserve(sources.size());
        for(Source alsrc : sources)
        {
            if(ALuint id = alsrc.getHandle()->getId())
                sourceids.push_back(id);
        }
        auto lock = mContext.getSourceStreamLock();
        alSourceRewindv(static_cast<ALsizei>(sourceids.size()), sourceids.data());
        throw_al_error("Failed to stop sources");
        for(Source alsrc : sources)
        {
            SourceImpl *source = alsrc.getHandle();
            mContext.removePendingSource(source);
            mContext.removeFadingSource(source);
            mContext.removePlayingSource(source);
            source->makeStopped(false);
            mContext.send(&MessageHandler::sourceForceStopped, source);
        }
        alGetError();
    }

    alDeleteBuffers(1, &mId);
    throw_al_error("Buffer failed to delete");
    mId = 0;
}


void BufferImpl::load(ALuint frames, ALenum format, SharedPtr<Decoder> decoder, ContextImpl *ctx)
{
    Vector<ALbyte> data(FramesToBytes(frames, mChannelConfig, mSampleType));

    ALuint got = decoder->read(data.data(), frames);
    if(got > 0)
    {
        frames = got;
        data.resize(FramesToBytes(frames, mChannelConfig, mSampleType));
    }
    else
    {
        ALbyte silence = 0;
        if(mSampleType == SampleType::UInt8) silence = -128;
        else if(mSampleType == SampleType::Mulaw) silence = 127;
        std::fill(data.begin(), data.end(), silence);
    }

    std::pair<uint64_t,uint64_t> loop_pts = decoder->getLoopPoints();
    if(loop_pts.first >= loop_pts.second)
        loop_pts = std::make_pair(0, frames);
    else
    {
        loop_pts.second = std::min<uint64_t>(loop_pts.second, frames);
        loop_pts.first = std::min<uint64_t>(loop_pts.first, loop_pts.second-1);
    }

    ctx->send(&MessageHandler::bufferLoading,
        mName, mChannelConfig, mSampleType, mFrequency, data
    );

    alBufferData(mId, format, data.data(), static_cast<ALsizei>(data.size()), mFrequency);
    if(ctx->hasExtension(AL::SOFT_loop_points))
    {
        ALint pts[2]{(ALint)loop_pts.first, (ALint)loop_pts.second};
        alBufferiv(mId, AL_LOOP_POINTS_SOFT, pts);
    }
}


DECL_THUNK0(ALuint, Buffer, getLength, const)
ALuint BufferImpl::getLength() const
{
    CheckContext(mContext);

    alGetError();
    ALint size=-1, bits=-1, chans=-1;
    alGetBufferi(mId, AL_SIZE, &size);
    alGetBufferi(mId, AL_BITS, &bits);
    alGetBufferi(mId, AL_CHANNELS, &chans);
    throw_al_error("Buffer format error");
    return size / chans * 8 / bits;
}

DECL_THUNK0(ALuint, Buffer, getSize, const)
ALuint BufferImpl::getSize() const
{
    CheckContext(mContext);

    alGetError();
    ALint size = -1;
    alGetBufferi(mId, AL_SIZE, &size);
    throw_al_error("Buffer size error");
    return size;
}

DECL_THUNK2(void, Buffer, setLoopPoints,, ALuint, ALuint)
void BufferImpl::setLoopPoints(ALuint start, ALuint end)
{
    ALuint length = getLength();

    if(UNLIKELY(!mSources.empty()))
        throw std::runtime_error("Buffer is in use");

    if(!mContext.hasExtension(AL::SOFT_loop_points))
    {
        if(start != 0 || end != length)
            throw std::runtime_error("Loop points not supported");
        return;
    }

    if(UNLIKELY(start >= end || end > length))
        throw std::domain_error("Loop points out of range");

    alGetError();
    ALint pts[2]{(ALint)start, (ALint)end};
    alBufferiv(mId, AL_LOOP_POINTS_SOFT, pts);
    throw_al_error("Failed to set loop points");
}

DECL_THUNK0(ALuintPair, Buffer, getLoopPoints, const)
std::pair<ALuint,ALuint> BufferImpl::getLoopPoints() const
{
    CheckContext(mContext);

    if(!mContext.hasExtension(AL::SOFT_loop_points))
        return std::make_pair(0, getLength());

    alGetError();
    ALint pts[2]{-1,-1};
    alGetBufferiv(mId, AL_LOOP_POINTS_SOFT, pts);
    throw_al_error("Failed to get loop points");

    return std::make_pair(pts[0], pts[1]);
}

DECL_THUNK0(ALuint, Buffer, getFrequency, const)
DECL_THUNK0(ChannelConfig, Buffer, getChannelConfig, const)
DECL_THUNK0(SampleType, Buffer, getSampleType, const)
DECL_THUNK0(Vector<Source>, Buffer, getSources, const)
DECL_THUNK0(StringView, Buffer, getName, const)
DECL_THUNK0(size_t, Buffer, getSourceCount, const)


ALURE_API const char *GetSampleTypeName(SampleType type)
{
    switch(type)
    {
        case SampleType::UInt8: return "Unsigned 8-bit";
        case SampleType::Int16: return "Signed 16-bit";
        case SampleType::Float32: return "32-bit float";
        case SampleType::Mulaw: return "Mulaw";
    }
    throw std::invalid_argument("Invalid type");
}

ALURE_API const char *GetChannelConfigName(ChannelConfig cfg)
{
    switch(cfg)
    {
        case ChannelConfig::Mono: return "Mono";
        case ChannelConfig::Stereo: return "Stereo";
        case ChannelConfig::Rear: return "Rear";
        case ChannelConfig::Quad: return "Quadrophonic";
        case ChannelConfig::X51: return "5.1 Surround";
        case ChannelConfig::X61: return "6.1 Surround";
        case ChannelConfig::X71: return "7.1 Surround";
        case ChannelConfig::BFormat2D: return "B-Format 2D";
        case ChannelConfig::BFormat3D: return "B-Format 3D";
    }
    throw std::invalid_argument("Invalid config");
}

ALURE_API ALuint FramesToBytes(ALuint frames, ChannelConfig chans, SampleType type)
{
    ALuint mult = 1;
    switch(chans)
    {
        case ChannelConfig::Mono: mult *= 1; break;
        case ChannelConfig::Stereo: mult *= 2; break;
        case ChannelConfig::Rear: mult *= 2; break;
        case ChannelConfig::Quad: mult *= 4; break;
        case ChannelConfig::X51: mult *= 6; break;
        case ChannelConfig::X61: mult *= 7; break;
        case ChannelConfig::X71: mult *= 8; break;
        case ChannelConfig::BFormat2D: mult *= 3; break;
        case ChannelConfig::BFormat3D: mult *= 4; break;
    }
    switch(type)
    {
        case SampleType::UInt8: mult *= 1; break;
        case SampleType::Int16: mult *= 2; break;
        case SampleType::Float32: mult *= 4; break;
        case SampleType::Mulaw: mult *= 1; break;
    }

    if(UNLIKELY(frames > std::numeric_limits<ALuint>::max()/mult))
        throw std::domain_error("Byte size result too large");
    return frames * mult;
}

ALURE_API ALuint BytesToFrames(ALuint bytes, ChannelConfig chans, SampleType type) noexcept
{
    ALuint size = bytes;
    switch(chans)
    {
        case ChannelConfig::Mono: size /= 1; break;
        case ChannelConfig::Stereo: size /= 2; break;
        case ChannelConfig::Rear: size /= 2; break;
        case ChannelConfig::Quad: size /= 4; break;
        case ChannelConfig::X51: size /= 6; break;
        case ChannelConfig::X61: size /= 7; break;
        case ChannelConfig::X71: size /= 8; break;
        case ChannelConfig::BFormat2D: size /= 3; break;
        case ChannelConfig::BFormat3D: size /= 4; break;
    }
    switch(type)
    {
        case SampleType::UInt8: size /= 1; break;
        case SampleType::Int16: size /= 2; break;
        case SampleType::Float32: size /= 4; break;
        case SampleType::Mulaw: size /= 1; break;
    }
    return size;
}


ALenum GetFormat(ChannelConfig chans, SampleType type)
{
    ContextImpl *ctx = ContextImpl::GetCurrent();

    auto fmtlist = std::lower_bound(std::begin(FormatLists), std::end(FormatLists), type,
        [](decltype(FormatLists[0]) &lhs, SampleType rhs) -> bool
        { return lhs.mType < rhs; }
    );
    for(;fmtlist != std::end(FormatLists) && fmtlist->mType == type;++fmtlist)
    {
        if(fmtlist->mType != type)
            continue;
        if(fmtlist->mExt != AL::EXTENSION_MAX && !ctx->hasExtension(fmtlist->mExt))
            continue;

        auto iter = std::lower_bound(
            fmtlist->mFormats.begin(), fmtlist->mFormats.end(), chans,
            [](const FormatListEntry &lhs, ChannelConfig rhs) -> bool
            { return lhs.mChannels < rhs; }
        );
        for(;iter != fmtlist->mFormats.end() && iter->mChannels == chans;++iter)
        {
            if(iter->mExt == AL::EXTENSION_MAX || ctx->hasExtension(iter->mExt))
            {
                ALenum e = alGetEnumValue(iter->mName);
                if(e != AL_NONE && e != -1) return e;
            }
        }
    }

    return AL_NONE;
}

} // namespace alure
