
#include "config.h"

#include "context.h"

#include <stdexcept>
#include <algorithm>
#include <functional>
#include <memory>
#include <iostream>
#include <fstream>
#include <cstring>
#include <map>
#include <new>

#include "alc.h"

#ifdef HAVE_WAVE
#include "decoders/wave.hpp"
#endif
#ifdef HAVE_VORBISFILE
#include "decoders/vorbisfile.hpp"
#endif
#ifdef HAVE_FLAC
#include "decoders/flac.hpp"
#endif
#ifdef HAVE_OPUSFILE
#include "decoders/opusfile.hpp"
#endif
#ifdef HAVE_LIBSNDFILE
#include "decoders/sndfile.hpp"
#endif
#ifdef HAVE_MINIMP3
#include "decoders/mp3.hpp"
#endif

#include "devicemanager.h"
#include "device.h"
#include "buffer.h"
#include "source.h"
#include "auxeffectslot.h"
#include "effect.h"
#include "sourcegroup.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace std {

// Implements a FNV-1a hash for StringView. NOTE: This is *NOT* guaranteed
// compatible with std::hash<String>! The standard does not give any specific
// hash implementation, nor a way for applications to access the same hash
// function as std::string (short of copying into a string and hashing that).
// So if you need Strings and StringViews to result in the same hash for the
// same set of characters, hash StringViews created from the Strings.
template<>
struct hash<alure::StringView> {
    size_t operator()(const alure::StringView &str) const noexcept
    {
        using traits_type = alure::StringView::traits_type;

        if /*constexpr*/ (sizeof(size_t) == 8)
        {
            static constexpr size_t hash_offset = 0xcbf29ce484222325;
            static constexpr size_t hash_prime = 0x100000001b3;

            size_t val = hash_offset;
            for(auto ch : str)
                val = (val^traits_type::to_int_type(ch)) * hash_prime;
            return val;
        }
        else
        {
            static constexpr size_t hash_offset = 0x811c9dc5;
            static constexpr size_t hash_prime = 0x1000193;

            size_t val = hash_offset;
            for(auto ch : str)
                val = (val^traits_type::to_int_type(ch)) * hash_prime;
            return val;
        }
    }
};

}

namespace {

// Global mutex to protect global context changes
std::mutex gGlobalCtxMutex;

#ifdef _WIN32
// Windows' std::ifstream fails with non-ANSI paths since the standard only
// specifies names using const char* (or std::string). MSVC has a non-standard
// extension using const wchar_t* (or std::wstring?) to handle Unicode paths,
// but not all Windows compilers support it. So we have to make our own istream
// that accepts UTF-8 paths and forwards to Unicode-aware I/O functions.
class StreamBuf final : public std::streambuf {
    alure::Array<char_type,4096> mBuffer;
    HANDLE mFile{INVALID_HANDLE_VALUE};

    int_type underflow() override
    {
        if(mFile != INVALID_HANDLE_VALUE && gptr() == egptr())
        {
            // Read in the next chunk of data, and set the pointers on success
            DWORD got = 0;
            if(ReadFile(mFile, mBuffer.data(), (DWORD)mBuffer.size(), &got, NULL))
                setg(mBuffer.data(), mBuffer.data(), mBuffer.data()+got);
        }
        if(gptr() == egptr())
            return traits_type::eof();
        return traits_type::to_int_type(*gptr());
    }

    pos_type seekoff(off_type offset, std::ios_base::seekdir whence, std::ios_base::openmode mode) override
    {
        if(mFile == INVALID_HANDLE_VALUE || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        LARGE_INTEGER fpos;
        switch(whence)
        {
            case std::ios_base::beg:
                fpos.QuadPart = offset;
                if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_BEGIN))
                    return traits_type::eof();
                break;

            case std::ios_base::cur:
                // If the offset remains in the current buffer range, just
                // update the pointer.
                if((offset >= 0 && offset < off_type(egptr()-gptr())) ||
                   (offset < 0 && -offset <= off_type(gptr()-eback())))
                {
                    // Get the current file offset to report the correct read
                    // offset.
                    fpos.QuadPart = 0;
                    if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_CURRENT))
                        return traits_type::eof();
                    setg(eback(), gptr()+offset, egptr());
                    return fpos.QuadPart - off_type(egptr()-gptr());
                }
                // Need to offset for the file offset being at egptr() while
                // the requested offset is relative to gptr().
                offset -= off_type(egptr()-gptr());
                fpos.QuadPart = offset;
                if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_CURRENT))
                    return traits_type::eof();
                break;

            case std::ios_base::end:
                fpos.QuadPart = offset;
                if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_END))
                    return traits_type::eof();
                break;

            default:
                return traits_type::eof();
        }
        setg(nullptr, nullptr, nullptr);
        return fpos.QuadPart;
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode mode) override
    {
        // Simplified version of seekoff
        if(mFile == INVALID_HANDLE_VALUE || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        LARGE_INTEGER fpos;
        fpos.QuadPart = pos;
        if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_BEGIN))
            return traits_type::eof();

        setg(nullptr, nullptr, nullptr);
        return fpos.QuadPart;
    }

public:
    bool open(const char *filename)
    {
        alure::Vector<wchar_t> wname;
        int wnamelen;

        wnamelen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
        if(wnamelen <= 0) return false;

        wname.resize(wnamelen);
        MultiByteToWideChar(CP_UTF8, 0, filename, -1, wname.data(), wnamelen);

        mFile = CreateFileW(wname.data(), GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(mFile == INVALID_HANDLE_VALUE) return false;
        return true;
    }

    bool is_open() const noexcept { return mFile != INVALID_HANDLE_VALUE; }

    StreamBuf() = default;
    ~StreamBuf() override
    {
        if(mFile != INVALID_HANDLE_VALUE)
            CloseHandle(mFile);
        mFile = INVALID_HANDLE_VALUE;
    }
};

// Inherit from std::istream to use our custom streambuf
class Stream final : public std::istream {
    StreamBuf mStreamBuf;

public:
    Stream(const char *filename) : std::istream(nullptr)
    {
        init(&mStreamBuf);

        // Set the failbit if the file failed to open.
        if(!mStreamBuf.open(filename)) clear(failbit);
    }

    bool is_open() const noexcept { return mStreamBuf.is_open(); }
};
#endif

using DecoderEntryPair = std::pair<alure::String,alure::UniquePtr<alure::DecoderFactory>>;
const DecoderEntryPair sDefaultDecoders[] = {
#ifdef HAVE_WAVE
    { "_alure_int_wave", alure::MakeUnique<alure::WaveDecoderFactory>() },
#endif
#ifdef HAVE_VORBISFILE
    { "_alure_int_vorbis", alure::MakeUnique<alure::VorbisFileDecoderFactory>() },
#endif
#ifdef HAVE_FLAC
    { "_alure_int_flac", alure::MakeUnique<alure::FlacDecoderFactory>() },
#endif
#ifdef HAVE_OPUSFILE
    { "_alure_int_opus", alure::MakeUnique<alure::OpusFileDecoderFactory>() },
#endif
#ifdef HAVE_LIBSNDFILE
    { "_alure_int_sndfile", alure::MakeUnique<alure::SndFileDecoderFactory>() },
#endif
#ifdef HAVE_MINIMP3
    { "_alure_int_minimp3", alure::MakeUnique<alure::Mp3DecoderFactory>() },
#endif
};
alure::Vector<DecoderEntryPair> sDecoders;


alure::DecoderOrExceptT GetDecoder(alure::UniquePtr<std::istream> &file,
                                   alure::ArrayView<DecoderEntryPair> decoders)
{
    while(!decoders.empty())
    {
        alure::DecoderFactory *factory = decoders.front().second.get();
        auto decoder = factory->createDecoder(file);
        if(decoder) return std::move(decoder);

        if(!file || !(file->clear(),file->seekg(0)))
            return std::make_exception_ptr(
                std::runtime_error("Failed to rewind file for the next decoder factory")
            );

        decoders = decoders.slice(1);
    }

    return alure::SharedPtr<alure::Decoder>(nullptr);
}

static alure::DecoderOrExceptT GetDecoder(alure::UniquePtr<std::istream> file)
{
    auto decoder = GetDecoder(file, sDecoders);
    if(std::holds_alternative<std::exception_ptr>(decoder)) return decoder;
    if(std::get<alure::SharedPtr<alure::Decoder>>(decoder)) return decoder;
    decoder = GetDecoder(file, sDefaultDecoders);
    if(std::holds_alternative<std::exception_ptr>(decoder)) return decoder;
    if(std::get<alure::SharedPtr<alure::Decoder>>(decoder)) return decoder;
    return (decoder = std::make_exception_ptr(std::runtime_error("No decoder found")));
}

class DefaultFileIOFactory final : public alure::FileIOFactory {
    alure::UniquePtr<std::istream> openFile(const alure::String &name) noexcept override
    {
#ifdef _WIN32
        auto file = alure::MakeUnique<Stream>(name.c_str());
#else
        auto file = alure::MakeUnique<std::ifstream>(name.c_str(), std::ios::binary);
#endif
        if(!file->is_open()) file = nullptr;
        return std::move(file);
    }
};
DefaultFileIOFactory sDefaultFileFactory;

alure::UniquePtr<alure::FileIOFactory> sFileFactory;

}

namespace alure {

static inline void CheckContext(const ContextImpl *ctx)
{
    auto count = ContextImpl::sContextSetCount.load(std::memory_order_acquire);
    if(UNLIKELY(count != ctx->mContextSetCounter))
    {
        if(UNLIKELY(ctx != ContextImpl::GetCurrent()))
            throw std::runtime_error("Called context is not current");
        ctx->mContextSetCounter = count;
    }
}

std::variant<std::monostate,uint64_t> ParseTimeval(StringView strval, double srate) noexcept
{
    try {
        size_t endpos;
        size_t cpos = strval.find_first_of(':');
        if(cpos == StringView::npos)
        {
            // No colon is present, treat it as a plain sample offset
            uint64_t val = std::stoull(String(strval), &endpos);
            if(endpos != strval.length()) return {};
            return val;
        }

        // Value is not a sample offset. Its format is [[HH:]MM]:SS[.sss] (at
        // least one colon must exist to be interpreted this way).
        uint64_t val = 0;

        if(cpos != 0)
        {
            // If a non-empty first value, parse it (may be hours or minutes)
            val = std::stoul(String(strval.data(), cpos), &endpos);
            if(endpos != cpos) return {};
        }

        strval = strval.substr(cpos+1);
        cpos = strval.find_first_of(':');
        if(cpos != StringView::npos)
        {
            // If a second colon is present, the first value was hours and this is
            // minutes, otherwise the first value was minutes.
            uint64_t val2 = 0;

            if(cpos != 0)
            {
                val2 = std::stoul(String(strval.data(), cpos), &endpos);
                if(endpos != cpos || val2 >= 60) return {};
            }

            // Combines hours and minutes into the full minute count
            if(val > std::numeric_limits<uint64_t>::max()/60)
                return {};
            val = val*60 + val2;
            strval = strval.substr(cpos+1);
        }

        double secs = 0.0;
        if(!strval.empty())
        {
            // Parse the seconds and its fraction. Only include the first 3 decimal
            // places for millisecond precision.
            size_t dpos = strval.find_first_of('.');
            String str = (dpos == StringView::npos) ?
                         String(strval) : String(strval.substr(0, dpos+4));
            secs = std::stod(str, &endpos);
            if(endpos != str.length() || !(secs >= 0.0 && secs < 60.0))
                return {};
        }

        // Convert minutes to seconds, add the seconds, then convert to samples.
        return static_cast<uint64_t>((val*60.0 + secs) * srate);
    }
    catch(...) {
    }

    return {};
}


Decoder::~Decoder() { }
DecoderFactory::~DecoderFactory() { }

void RegisterDecoder(StringView name, UniquePtr<DecoderFactory> factory)
{
    auto iter = std::lower_bound(sDecoders.begin(), sDecoders.end(), name,
        [](const DecoderEntryPair &entry, StringView rhs) -> bool
        { return entry.first < rhs; }
    );
    if(iter != sDecoders.end())
        throw std::runtime_error("Decoder factory already registered");
    sDecoders.insert(iter, std::make_pair(String(name), std::move(factory)));
}

UniquePtr<DecoderFactory> UnregisterDecoder(StringView name) noexcept
{
    UniquePtr<DecoderFactory> factory;
    auto iter = std::lower_bound(sDecoders.begin(), sDecoders.end(), name,
        [](const DecoderEntryPair &entry, StringView rhs) noexcept -> bool
        { return entry.first < rhs; }
    );
    if(iter != sDecoders.end())
    {
        factory = std::move(iter->second);
        sDecoders.erase(iter);
        return factory;
    }
    return factory;
}


FileIOFactory::~FileIOFactory() { }

UniquePtr<FileIOFactory> FileIOFactory::set(UniquePtr<FileIOFactory> factory) noexcept
{
    sFileFactory.swap(factory);
    return factory;
}

FileIOFactory &FileIOFactory::get() noexcept
{
    FileIOFactory *factory = sFileFactory.get();
    if(factory) return *factory;
    return sDefaultFileFactory;
}


// Default message handler methods are no-ops.
MessageHandler::~MessageHandler()
{
}

void MessageHandler::deviceDisconnected(Device) noexcept
{
}

void MessageHandler::sourceStopped(Source) noexcept
{
}

void MessageHandler::sourceForceStopped(Source) noexcept
{
}

void MessageHandler::bufferLoading(StringView, ChannelConfig, SampleType, ALuint, ArrayView<ALbyte>) noexcept
{
}

String MessageHandler::resourceNotFound(StringView) noexcept
{
    return String();
}


template<typename T>
static inline void LoadALFunc(T **func, const char *name)
{ *func = reinterpret_cast<T*>(alGetProcAddress(name)); }

static void LoadNothing(ContextImpl*) { }

static void LoadEFX(ContextImpl *ctx)
{
    LoadALFunc(&ctx->alGenEffects,    "alGenEffects");
    LoadALFunc(&ctx->alDeleteEffects, "alDeleteEffects");
    LoadALFunc(&ctx->alIsEffect,      "alIsEffect");
    LoadALFunc(&ctx->alEffecti,       "alEffecti");
    LoadALFunc(&ctx->alEffectiv,      "alEffectiv");
    LoadALFunc(&ctx->alEffectf,       "alEffectf");
    LoadALFunc(&ctx->alEffectfv,      "alEffectfv");
    LoadALFunc(&ctx->alGetEffecti,    "alGetEffecti");
    LoadALFunc(&ctx->alGetEffectiv,   "alGetEffectiv");
    LoadALFunc(&ctx->alGetEffectf,    "alGetEffectf");
    LoadALFunc(&ctx->alGetEffectfv,   "alGetEffectfv");

    LoadALFunc(&ctx->alGenFilters,    "alGenFilters");
    LoadALFunc(&ctx->alDeleteFilters, "alDeleteFilters");
    LoadALFunc(&ctx->alIsFilter,      "alIsFilter");
    LoadALFunc(&ctx->alFilteri,       "alFilteri");
    LoadALFunc(&ctx->alFilteriv,      "alFilteriv");
    LoadALFunc(&ctx->alFilterf,       "alFilterf");
    LoadALFunc(&ctx->alFilterfv,      "alFilterfv");
    LoadALFunc(&ctx->alGetFilteri,    "alGetFilteri");
    LoadALFunc(&ctx->alGetFilteriv,   "alGetFilteriv");
    LoadALFunc(&ctx->alGetFilterf,    "alGetFilterf");
    LoadALFunc(&ctx->alGetFilterfv,   "alGetFilterfv");

    LoadALFunc(&ctx->alGenAuxiliaryEffectSlots,    "alGenAuxiliaryEffectSlots");
    LoadALFunc(&ctx->alDeleteAuxiliaryEffectSlots, "alDeleteAuxiliaryEffectSlots");
    LoadALFunc(&ctx->alIsAuxiliaryEffectSlot,      "alIsAuxiliaryEffectSlot");
    LoadALFunc(&ctx->alAuxiliaryEffectSloti,       "alAuxiliaryEffectSloti");
    LoadALFunc(&ctx->alAuxiliaryEffectSlotiv,      "alAuxiliaryEffectSlotiv");
    LoadALFunc(&ctx->alAuxiliaryEffectSlotf,       "alAuxiliaryEffectSlotf");
    LoadALFunc(&ctx->alAuxiliaryEffectSlotfv,      "alAuxiliaryEffectSlotfv");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSloti,    "alGetAuxiliaryEffectSloti");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSlotiv,   "alGetAuxiliaryEffectSlotiv");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSlotf,    "alGetAuxiliaryEffectSlotf");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSlotfv,   "alGetAuxiliaryEffectSlotfv");
}

static void LoadSourceResampler(ContextImpl *ctx)
{
    LoadALFunc(&ctx->alGetStringiSOFT, "alGetStringiSOFT");
}

static void LoadSourceLatency(ContextImpl *ctx)
{
    LoadALFunc(&ctx->alGetSourcei64vSOFT, "alGetSourcei64vSOFT");
    LoadALFunc(&ctx->alGetSourcedvSOFT, "alGetSourcedvSOFT");
}

static const struct {
    AL extension;
    const char name[32];
    void (&loader)(ContextImpl*);
} ALExtensionList[] = {
    { AL::EXT_EFX, "ALC_EXT_EFX", LoadEFX },

    { AL::EXT_FLOAT32,   "AL_EXT_FLOAT32",   LoadNothing },
    { AL::EXT_MCFORMATS, "AL_EXT_MCFORMATS", LoadNothing },
    { AL::EXT_BFORMAT,   "AL_EXT_BFORMAT",   LoadNothing },

    { AL::EXT_MULAW,           "AL_EXT_MULAW",           LoadNothing },
    { AL::EXT_MULAW_MCFORMATS, "AL_EXT_MULAW_MCFORMATS", LoadNothing },
    { AL::EXT_MULAW_BFORMAT,   "AL_EXT_MULAW_BFORMAT",   LoadNothing },

    { AL::SOFT_loop_points,       "AL_SOFT_loop_points",       LoadNothing },
    { AL::SOFT_source_latency,    "AL_SOFT_source_latency",    LoadSourceLatency },
    { AL::SOFT_source_resampler,  "AL_SOFT_source_resampler",  LoadSourceResampler },
    { AL::SOFT_source_spatialize, "AL_SOFT_source_spatialize", LoadNothing },

    { AL::EXT_disconnect, "ALC_EXT_disconnect", LoadNothing },

    { AL::EXT_SOURCE_RADIUS, "AL_EXT_SOURCE_RADIUS", LoadNothing },
    { AL::EXT_STEREO_ANGLES, "AL_EXT_STEREO_ANGLES", LoadNothing },
};


ContextImpl *ContextImpl::sCurrentCtx = nullptr;
thread_local ContextImpl *ContextImpl::sThreadCurrentCtx = nullptr;

std::atomic<uint64_t> ContextImpl::sContextSetCount{0};

void ContextImpl::MakeCurrent(ContextImpl *context)
{
    std::unique_lock<std::mutex> ctxlock(gGlobalCtxMutex);

    if(alcMakeContextCurrent(context ? context->getALCcontext() : nullptr) == ALC_FALSE)
        throw std::runtime_error("Call to alcMakeContextCurrent failed");
    if(context)
    {
        context->addRef();
        std::call_once(context->mSetExts, std::mem_fn(&ContextImpl::setupExts), context);
    }
    std::swap(sCurrentCtx, context);
    if(context) context->decRef();

    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = nullptr;
    sContextSetCount.fetch_add(1, std::memory_order_release);

    if((context = sCurrentCtx) != nullptr)
    {
        ctxlock.unlock();
        context->mWakeThread.notify_all();
    }
}

void ContextImpl::MakeThreadCurrent(ContextImpl *context)
{
    if(!DeviceManagerImpl::SetThreadContext)
        throw std::runtime_error("Thread-local contexts unsupported");
    if(DeviceManagerImpl::SetThreadContext(context ? context->getALCcontext() : nullptr) == ALC_FALSE)
        throw std::runtime_error("Call to alcSetThreadContext failed");
    if(context)
    {
        context->addRef();
        std::call_once(context->mSetExts, std::mem_fn(&ContextImpl::setupExts), context);
    }
    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = context;
    sContextSetCount.fetch_add(1, std::memory_order_release);
}

void ContextImpl::setupExts()
{
    ALCdevice *device = mDevice.getALCdevice();
    for(const auto &entry : ALExtensionList)
    {
        if((strncmp(entry.name, "ALC", 3) == 0) ? alcIsExtensionPresent(device, entry.name) :
                                                  alIsExtensionPresent(entry.name))
        {
            mHasExt.set(static_cast<size_t>(entry.extension));
            entry.loader(this);
        }
    }
}


void ContextImpl::backgroundProc()
{
    if(DeviceManagerImpl::SetThreadContext && mDevice.hasExtension(ALC::EXT_thread_local_context))
        DeviceManagerImpl::SetThreadContext(getALCcontext());

    std::chrono::steady_clock::time_point basetime = std::chrono::steady_clock::now();
    std::chrono::milliseconds waketime(0);
    std::unique_lock<std::mutex> ctxlock(gGlobalCtxMutex);
    while(!mQuitThread.load(std::memory_order_acquire))
    {
        {
            std::lock_guard<std::mutex> srclock(mSourceStreamMutex);
            mStreamingSources.erase(
                std::remove_if(mStreamingSources.begin(), mStreamingSources.end(),
                    [](SourceImpl *source) -> bool
                    { return !source->updateAsync(); }
                ), mStreamingSources.end()
            );
        }

        // Only do one pending buffer at a time. In case there's several large
        // buffers to load, we still need to process streaming sources so they
        // don't underrun.
        PendingPromise *lastpb = mPendingCurrent.load(std::memory_order_acquire);
        if(PendingPromise *pb = lastpb->mNext.load(std::memory_order_relaxed))
        {
            pb->mBuffer->load(pb->mFrames, pb->mFormat, std::move(pb->mDecoder), this);
            pb->mPromise.set_value(Buffer(pb->mBuffer));
            Promise<Buffer>().swap(pb->mPromise);
            mPendingCurrent.store(pb, std::memory_order_release);
            continue;
        }

        std::unique_lock<std::mutex> wakelock(mWakeMutex);
        if(!mQuitThread.load(std::memory_order_acquire) && lastpb->mNext.load(std::memory_order_acquire) == nullptr)
        {
            ctxlock.unlock();

            std::chrono::milliseconds interval = mWakeInterval.load(std::memory_order_relaxed);
            if(interval.count() == 0)
                mWakeThread.wait(wakelock);
            else
            {
                auto now = std::chrono::steady_clock::now() - basetime;
                if(now > waketime)
                {
                    auto mult = (now-waketime + interval-std::chrono::milliseconds(1)) / interval;
                    waketime += interval * mult;
                }
                mWakeThread.wait_until(wakelock, waketime + basetime);
            }
            wakelock.unlock();

            ctxlock.lock();
            while(!mQuitThread.load(std::memory_order_acquire) &&
                  alcGetCurrentContext() != getALCcontext())
                mWakeThread.wait(ctxlock);
        }
    }
    ctxlock.unlock();

    if(DeviceManagerImpl::SetThreadContext)
        DeviceManagerImpl::SetThreadContext(nullptr);
}


ContextImpl::ContextImpl(DeviceImpl &device, ArrayView<AttributePair> attrs)
  : mListener(this), mDevice(device), mIsConnected(true), mIsBatching(false)
{
    ALCdevice *alcdev = mDevice.getALCdevice();
    if(attrs.empty()) /* No explicit attributes. */
        mContext.reset(alcCreateContext(alcdev, nullptr));
    else
        mContext.reset(alcCreateContext(alcdev, &attrs.front().mAttribute));
    if(!mContext) throw alc_error(alcGetError(alcdev), "alcCreateContext failed");

    mSourceIds.reserve(256);
    mPendingTail = mPendingHead = new PendingPromise();
    mPendingCurrent.store(mPendingHead, std::memory_order_relaxed);
}

ContextImpl::~ContextImpl()
{
    if(mThread.joinable())
    {
        std::unique_lock<std::mutex> lock(mWakeMutex);
        mQuitThread.store(true, std::memory_order_relaxed);
        lock.unlock();
        mWakeThread.notify_all();
        mThread.join();
    }

    PendingPromise *pb = mPendingTail;
    while(pb)
    {
        PendingPromise *next = pb->mNext.load(std::memory_order_relaxed);
        delete pb;
        pb = next;
    }
    mPendingCurrent.store(nullptr, std::memory_order_relaxed);
    mPendingTail = mPendingHead = nullptr;

    mEffectSlots.clear();
    mEffects.clear();

    std::lock_guard<std::mutex> ctxlock(gGlobalCtxMutex);
    if(sCurrentCtx == this)
    {
        sCurrentCtx = nullptr;
        sContextSetCount.fetch_add(1, std::memory_order_release);
    }
    if(sThreadCurrentCtx == this)
    {
        sThreadCurrentCtx = nullptr;
        sContextSetCount.fetch_add(1, std::memory_order_release);
    }
}


void Context::destroy()
{
    ContextImpl *i = pImpl;
    pImpl = nullptr;
    i->destroy();
}
void ContextImpl::destroy()
{
    if(mRefs != 0)
    {
        std::lock_guard<std::mutex> ctxlock(gGlobalCtxMutex);
        if(!(mRefs == 1 && sCurrentCtx == this))
            throw std::runtime_error("Context is in use");
        decRef();
        sCurrentCtx = nullptr;
        sContextSetCount.fetch_add(1, std::memory_order_release);
    }

    if(mThread.joinable())
    {
        std::unique_lock<std::mutex> lock(mWakeMutex);
        mQuitThread.store(true, std::memory_order_release);
        lock.unlock();
        mWakeThread.notify_all();
        mThread.join();
    }

    std::unique_lock<std::mutex> lock(gGlobalCtxMutex);
    if(UNLIKELY(alcMakeContextCurrent(getALCcontext()) == ALC_FALSE))
        std::cerr<< "Failed to cleanup context!" <<std::endl;
    else
    {
        mSourceGroups.clear();
        mFreeSources.clear();
        mAllSources.clear();

        if(!mSourceIds.empty())
            alDeleteSources(static_cast<ALsizei>(mSourceIds.size()), mSourceIds.data());
        mSourceIds.clear();

        for(auto &bufptr : mBuffers)
        {
            ALuint id = bufptr->getId();
            alDeleteBuffers(1, &id);
        }
        mBuffers.clear();

        mEffectSlots.clear();
        mEffects.clear();

        ALCcontext *alctx = sCurrentCtx ? sCurrentCtx->getALCcontext() : nullptr;
        if(UNLIKELY(alcMakeContextCurrent(alctx) == ALC_FALSE))
            std::cerr<< "Failed to reset global context!" <<std::endl;
        if(ContextImpl *thrd_ctx = sThreadCurrentCtx)
        {
            // alcMakeContextCurrent sets the calling thread's context to null,
            // set it back to what it was.
            alctx = thrd_ctx->getALCcontext();
            if(UNLIKELY(DeviceManagerImpl::SetThreadContext(alctx) == ALC_FALSE))
                std::cerr<< "Failed to reset thread context!" <<std::endl;
        }
    }
    lock.unlock();

    mDevice.removeContext(this);
}


DECL_THUNK0(void, Context, startBatch,)
void ContextImpl::startBatch()
{
    alcSuspendContext(mContext.get());
    mIsBatching = true;
}

DECL_THUNK0(void, Context, endBatch,)
void ContextImpl::endBatch()
{
    alcProcessContext(mContext.get());
    mIsBatching = false;
}


DECL_THUNK1(SharedPtr<MessageHandler>, Context, setMessageHandler,, SharedPtr<MessageHandler>)
SharedPtr<MessageHandler> ContextImpl::setMessageHandler(SharedPtr<MessageHandler>&& handler)
{
    std::lock_guard<std::mutex> lock(gGlobalCtxMutex);
    mMessage.swap(handler);
    return handler;
}


DECL_THUNK1(void, Context, setAsyncWakeInterval,, std::chrono::milliseconds)
void ContextImpl::setAsyncWakeInterval(std::chrono::milliseconds interval)
{
    if(interval.count() < 0 || interval > std::chrono::seconds(1))
        throw std::domain_error("Async wake interval out of range");
    mWakeInterval.store(interval);
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();
}


DecoderOrExceptT ContextImpl::findDecoder(StringView name)
{
    String oldname = String(name);
    auto file = FileIOFactory::get().openFile(oldname);
    if(UNLIKELY(!file))
    {
        // Resource not found. Try to find a substitute.
        if(!mMessage.get())
            return std::make_exception_ptr(std::runtime_error("Failed to open file"));
        do {
            String newname(mMessage->resourceNotFound(oldname));
            if(newname.empty())
                return std::make_exception_ptr(std::runtime_error("Failed to open file"));
            file = FileIOFactory::get().openFile(newname);
            oldname = std::move(newname);
        } while(!file);
    }
    return GetDecoder(std::move(file));
}

DECL_THUNK1(SharedPtr<Decoder>, Context, createDecoder,, StringView)
SharedPtr<Decoder> ContextImpl::createDecoder(StringView name)
{
    CheckContext(this);
    DecoderOrExceptT dec = findDecoder(name);
    if(SharedPtr<Decoder> *decoder = std::get_if<SharedPtr<Decoder>>(&dec))
        return std::move(*decoder);
    std::rethrow_exception(std::get<std::exception_ptr>(dec));
}


DECL_THUNK2(bool, Context, isSupported, const, ChannelConfig, SampleType)
bool ContextImpl::isSupported(ChannelConfig channels, SampleType type) const
{
    CheckContext(this);
    return GetFormat(channels, type) != AL_NONE;
}


DECL_THUNK0(ArrayView<String>, Context, getAvailableResamplers,)
ArrayView<String> ContextImpl::getAvailableResamplers()
{
    CheckContext(this);
    if(mResamplers.empty() && hasExtension(AL::SOFT_source_resampler))
    {
        ALint num_resamplers = alGetInteger(AL_NUM_RESAMPLERS_SOFT);
        mResamplers.reserve(num_resamplers);
        for(int i = 0;i < num_resamplers;i++)
            mResamplers.emplace_back(alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i));
        if(mResamplers.empty())
            mResamplers.emplace_back();
    }
    return mResamplers;
}

DECL_THUNK0(ALsizei, Context, getDefaultResamplerIndex, const)
ALsizei ContextImpl::getDefaultResamplerIndex() const
{
    CheckContext(this);
    if(!hasExtension(AL::SOFT_source_resampler))
        return 0;
    return alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);
}


ContextImpl::FutureBufferListT::const_iterator ContextImpl::findFutureBufferName(StringView name, size_t name_hash) const
{
    auto iter = std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), name_hash,
        [](const PendingBuffer &lhs, size_t rhs) -> bool
        { return lhs.mBuffer->getNameHash() < rhs; }
    );
    while(iter != mFutureBuffers.end() && iter->mBuffer->getNameHash() == name_hash &&
          iter->mBuffer->getName() != name)
        ++iter;
    return iter;
}

ContextImpl::BufferListT::const_iterator ContextImpl::findBufferName(StringView name, size_t name_hash) const
{
    auto iter = std::lower_bound(mBuffers.begin(), mBuffers.end(), name_hash,
        [](const UniquePtr<BufferImpl> &lhs, size_t rhs) -> bool
        { return lhs->getNameHash() < rhs; }
    );
    while(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash &&
          (*iter)->getName() != name)
        ++iter;
    return iter;
}

BufferOrExceptT ContextImpl::doCreateBuffer(StringView name, size_t name_hash, BufferListT::const_iterator iter, SharedPtr<Decoder> decoder)
{
    ALuint srate = decoder->getFrequency();
    ChannelConfig chans = decoder->getChannelConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = static_cast<ALuint>(
        std::min<uint64_t>(decoder->getLength(), std::numeric_limits<ALuint>::max())
    );

    Vector<ALbyte> data(FramesToBytes(frames, chans, type));
    frames = decoder->read(data.data(), frames);
    if(!frames)
        return std::make_exception_ptr(std::runtime_error("No samples for buffer"));
    data.resize(FramesToBytes(frames, chans, type));

    std::pair<uint64_t,uint64_t> loop_pts = decoder->getLoopPoints();
    if(loop_pts.first >= loop_pts.second)
        loop_pts = std::make_pair(0, frames);
    else
    {
        loop_pts.second = std::min<uint64_t>(loop_pts.second, frames);
        loop_pts.first = std::min<uint64_t>(loop_pts.first, loop_pts.second-1);
    }

    // Get the format before calling the bufferLoading message handler, to
    // ensure it's something OpenAL can handle.
    ALenum format = GetFormat(chans, type);
    if(UNLIKELY(format == AL_NONE))
    {
        auto str = String("Unsupported format (")+GetSampleTypeName(type)+", "+
                   GetChannelConfigName(chans)+")";
        return std::make_exception_ptr(std::runtime_error(str));
    }

    if(mMessage.get())
        mMessage->bufferLoading(name, chans, type, srate, data);

    alGetError();
    ALuint bid = 0;
    alGenBuffers(1, &bid);
    alBufferData(bid, format, data.data(), static_cast<ALsizei>(data.size()), srate);
    if(hasExtension(AL::SOFT_loop_points))
    {
        ALint pts[2]{(ALint)loop_pts.first, (ALint)loop_pts.second};
        alBufferiv(bid, AL_LOOP_POINTS_SOFT, pts);
    }
    if(ALenum err = alGetError())
    {
        alDeleteBuffers(1, &bid);
        return std::make_exception_ptr(al_error(err, "Failed to buffer data"));
    }

    return mBuffers.insert(iter,
        MakeUnique<BufferImpl>(*this, bid, srate, chans, type, name, name_hash)
    )->get();
}

BufferOrExceptT ContextImpl::doCreateBufferAsync(StringView name, size_t name_hash, BufferListT::const_iterator iter, SharedPtr<Decoder> decoder, Promise<Buffer> promise)
{
    ALuint srate = decoder->getFrequency();
    ChannelConfig chans = decoder->getChannelConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = static_cast<ALuint>(
        std::min<uint64_t>(decoder->getLength(), std::numeric_limits<ALuint>::max())
    );
    if(!frames)
        return std::make_exception_ptr(std::runtime_error("No samples for buffer"));

    ALenum format = GetFormat(chans, type);
    if(UNLIKELY(format == AL_NONE))
    {
        auto str = String("Unsupported format (")+GetSampleTypeName(type)+", "+
                   GetChannelConfigName(chans)+")";
        return std::make_exception_ptr(std::runtime_error(str));
    }

    alGetError();
    ALuint bid = 0;
    alGenBuffers(1, &bid);
    if(ALenum err = alGetError())
        return std::make_exception_ptr(al_error(err, "Failed to create buffer"));

    auto buffer = MakeUnique<BufferImpl>(*this, bid, srate, chans, type, name, name_hash);

    if(mThread.get_id() == std::thread::id())
        mThread = std::thread(std::mem_fn(&ContextImpl::backgroundProc), this);

    PendingPromise *pf = nullptr;
    if(mPendingTail == mPendingCurrent.load(std::memory_order_acquire))
        pf = new PendingPromise(buffer.get(), std::move(decoder), format, frames,
                                std::move(promise));
    else
    {
        pf = mPendingTail;
        pf->mBuffer = buffer.get();
        pf->mDecoder = std::move(decoder);
        pf->mFormat = format;
        pf->mFrames = frames;
        pf->mPromise = std::move(promise);
        mPendingTail = pf->mNext.exchange(nullptr, std::memory_order_relaxed);
    }

    mPendingHead->mNext.store(pf, std::memory_order_release);
    mPendingHead = pf;

    return mBuffers.insert(iter, std::move(buffer))->get();
}

DECL_THUNK1(Buffer, Context, getBuffer,, StringView)
Buffer ContextImpl::getBuffer(StringView name)
{
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    size_t name_hash = hasher(name);
    if(UNLIKELY(!mFutureBuffers.empty()))
    {
        Buffer buffer;

        // If the buffer is already pending for the future, wait for it
        auto iter = findFutureBufferName(name, name_hash);
        if(iter != mFutureBuffers.end() && iter->mBuffer->getNameHash() == name_hash)
        {
            buffer = iter->mFuture.get();
            mFutureBuffers.erase(iter);
        }

        // Clear out any completed futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingBuffer &entry) -> bool
                { return GetFutureState(entry.mFuture) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );

        // If we got the buffer, return it. Otherwise, go load it normally.
        if(buffer) return buffer;
    }

    auto iter = findBufferName(name, name_hash);
    if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
        return Buffer(iter->get());

    BufferOrExceptT ret = doCreateBuffer(name, name_hash, iter, createDecoder(name));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(UNLIKELY(!buffer))
        std::rethrow_exception(std::get<std::exception_ptr>(ret));
    return *buffer;
}

DECL_THUNK1(SharedFuture<Buffer>, Context, getBufferAsync,, StringView)
SharedFuture<Buffer> ContextImpl::getBufferAsync(StringView name)
{
    SharedFuture<Buffer> future;
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    size_t name_hash = hasher(name);
    if(UNLIKELY(!mFutureBuffers.empty()))
    {
        // Check if the future that's being created already exists
        auto iter = findFutureBufferName(name, name_hash);
        if(iter != mFutureBuffers.end() && iter->mBuffer->getNameHash() == name_hash)
        {
            future = iter->mFuture;
            if(GetFutureState(future) == std::future_status::ready)
                mFutureBuffers.erase(iter);
            return future;
        }

        // Clear out any fulfilled futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingBuffer &entry) -> bool
                { return GetFutureState(entry.mFuture) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto iter = findBufferName(name, name_hash);
    if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
    {
        // User asked to create a future buffer that's already loaded. Just
        // construct a promise, fulfill the promise immediately, then return a
        // shared future that's already set.
        Promise<Buffer> promise;
        promise.set_value(Buffer(iter->get()));
        future = promise.get_future().share();
        return future;
    }

    Promise<Buffer> promise;
    future = promise.get_future().share();

    BufferOrExceptT ret = doCreateBufferAsync(name, name_hash, iter, createDecoder(name), std::move(promise));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(UNLIKELY(!buffer))
        std::rethrow_exception(std::get<std::exception_ptr>(ret));
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();

    mFutureBuffers.insert(
        std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), name_hash,
            [](const PendingBuffer &lhs, size_t rhs) -> bool
            { return lhs.mBuffer->getNameHash() < rhs; }
        ), { buffer->getHandle(), future }
    );

    return future;
}

DECL_THUNK1(void, Context, precacheBuffersAsync,, ArrayView<StringView>)
void ContextImpl::precacheBuffersAsync(ArrayView<StringView> names)
{
    CheckContext(this);

    if(UNLIKELY(!mFutureBuffers.empty()))
    {
        // Clear out any fulfilled futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingBuffer &entry) -> bool
                { return GetFutureState(entry.mFuture) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto hasher = std::hash<StringView>();
    for(const StringView name : names)
    {
        size_t name_hash = hasher(name);

        // Check if the buffer that's being created already exists
        auto iter = findBufferName(name, name_hash);
        if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
            continue;

        DecoderOrExceptT dec = findDecoder(name);
        SharedPtr<Decoder> *decoder = std::get_if<SharedPtr<Decoder>>(&dec);
        if(!decoder) continue;

        Promise<Buffer> promise;
        SharedFuture<Buffer> future = promise.get_future().share();

        BufferOrExceptT buf = doCreateBufferAsync(name, name_hash, iter, std::move(*decoder),
                                                  std::move(promise));
        Buffer *buffer = std::get_if<Buffer>(&buf);
        if(UNLIKELY(!buffer)) continue;

        mFutureBuffers.insert(
            std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), name_hash,
                [](const PendingBuffer &lhs, size_t rhs) -> bool
                { return lhs.mBuffer->getNameHash() < rhs; }
            ), { buffer->getHandle(), future }
        );
    }
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();
}

DECL_THUNK2(Buffer, Context, createBufferFrom,, StringView, SharedPtr<Decoder>)
Buffer ContextImpl::createBufferFrom(StringView name, SharedPtr<Decoder>&& decoder)
{
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    size_t name_hash = hasher(name);
    auto iter = findBufferName(name, name_hash);
    if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
        throw std::runtime_error("Buffer already exists");

    BufferOrExceptT ret = doCreateBuffer(name, name_hash, iter, std::move(decoder));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(UNLIKELY(!buffer))
        std::rethrow_exception(std::get<std::exception_ptr>(ret));
    return *buffer;
}

DECL_THUNK2(SharedFuture<Buffer>, Context, createBufferAsyncFrom,, StringView, SharedPtr<Decoder>)
SharedFuture<Buffer> ContextImpl::createBufferAsyncFrom(StringView name, SharedPtr<Decoder>&& decoder)
{
    SharedFuture<Buffer> future;
    CheckContext(this);

    if(UNLIKELY(!mFutureBuffers.empty()))
    {
        // Clear out any fulfilled futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingBuffer &entry) -> bool
                { return GetFutureState(entry.mFuture) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto hasher = std::hash<StringView>();
    size_t name_hash = hasher(name);
    auto iter = findBufferName(name, name_hash);
    if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
        throw std::runtime_error("Buffer already exists");

    Promise<Buffer> promise;
    future = promise.get_future().share();

    BufferOrExceptT ret = doCreateBufferAsync(name, name_hash, iter, std::move(decoder), std::move(promise));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(UNLIKELY(!buffer))
        std::rethrow_exception(std::get<std::exception_ptr>(ret));
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();

    mFutureBuffers.insert(
        std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), name_hash,
            [](const PendingBuffer &lhs, size_t rhs) -> bool
            { return lhs.mBuffer->getNameHash() < rhs; }
        ), { buffer->getHandle(), future }
    );

    return future;
}


DECL_THUNK1(Buffer, Context, findBuffer,, StringView)
Buffer ContextImpl::findBuffer(StringView name)
{
    Buffer buffer;
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    size_t name_hash = hasher(name);
    if(UNLIKELY(!mFutureBuffers.empty()))
    {
        // If the buffer is already pending for the future, wait for it
        auto iter = findFutureBufferName(name, name_hash);
        if(iter != mFutureBuffers.end() && iter->mBuffer->getNameHash() == name_hash)
        {
            buffer = iter->mFuture.get();
            mFutureBuffers.erase(iter);
        }

        // Clear out any completed futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingBuffer &entry) -> bool
                { return GetFutureState(entry.mFuture) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    if(LIKELY(!buffer))
    {
        auto iter = findBufferName(name, name_hash);
        if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
            buffer = Buffer(iter->get());
    }
    return buffer;
}

DECL_THUNK1(SharedFuture<Buffer>, Context, findBufferAsync,, StringView)
SharedFuture<Buffer> ContextImpl::findBufferAsync(StringView name)
{
    SharedFuture<Buffer> future;
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    size_t name_hash = hasher(name);
    if(UNLIKELY(!mFutureBuffers.empty()))
    {
        // Check if the future that's being created already exists
        auto iter = findFutureBufferName(name, name_hash);
        if(iter != mFutureBuffers.end() && iter->mBuffer->getNameHash() == name_hash)
        {
            future = iter->mFuture;
            if(GetFutureState(future) == std::future_status::ready)
                mFutureBuffers.erase(iter);
            return future;
        }

        // Clear out any fulfilled futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingBuffer &entry) -> bool
                { return GetFutureState(entry.mFuture) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto iter = findBufferName(name, name_hash);
    if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
    {
        // User asked to create a future buffer that's already loaded. Just
        // construct a promise, fulfill the promise immediately, then return a
        // shared future that's already set.
        Promise<Buffer> promise;
        promise.set_value(Buffer(iter->get()));
        future = promise.get_future().share();
    }
    return future;
}


DECL_THUNK1(void, Context, removeBuffer,, Buffer)
DECL_THUNK1(void, Context, removeBuffer,, StringView)
void ContextImpl::removeBuffer(StringView name)
{
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    size_t name_hash = hasher(name);
    if(UNLIKELY(!mFutureBuffers.empty()))
    {
        // If the buffer is already pending for the future, wait for it to
        // finish before continuing.
        auto iter = findFutureBufferName(name, name_hash);
        if(iter != mFutureBuffers.end() && iter->mBuffer->getNameHash() == name_hash)
        {
            iter->mFuture.wait();
            mFutureBuffers.erase(iter);
        }

        // Clear out any completed futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingBuffer &entry) -> bool
                { return GetFutureState(entry.mFuture) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto iter = findBufferName(name, name_hash);
    if(iter != mBuffers.end() && (*iter)->getNameHash() == name_hash)
    {
        // Remove pending sources whose future was waiting for this buffer.
        BufferImpl *buffer = iter->get();
        mPendingSources.erase(
            std::remove_if(mPendingSources.begin(), mPendingSources.end(),
                [buffer](PendingSource &entry) -> bool
                {
                    return (GetFutureState(entry.mFuture) == std::future_status::ready &&
                            entry.mFuture.get().getHandle() == buffer);
                }
            ), mPendingSources.end()
        );
        (*iter)->cleanup();
        mBuffers.erase(iter);
    }
}


ALuint ContextImpl::getSourceId(ALuint maxprio)
{
    ALuint id = 0;
    if(mSourceIds.empty())
    {
        alGetError();
        alGenSources(1, &id);
        if(alGetError() == AL_NO_ERROR)
            return id;

        SourceImpl *lowest = nullptr;
        for(SourceBufferUpdateEntry &entry : mPlaySources)
        {
            if(!lowest || entry.mSource->getPriority() < lowest->getPriority())
                lowest = entry.mSource;
        }
        for(SourceStreamUpdateEntry &entry : mStreamSources)
        {
            if(!lowest || entry.mSource->getPriority() < lowest->getPriority())
                lowest = entry.mSource;
        }
        if(lowest && lowest->getPriority() < maxprio)
        {
            lowest->stop();
            if(mMessage.get())
                mMessage->sourceForceStopped(lowest);
        }
    }
    if(mSourceIds.empty())
        throw std::runtime_error("No available sources");

    id = mSourceIds.back();
    mSourceIds.pop_back();
    return id;
}


DECL_THUNK0(Source, Context, createSource,)
Source ContextImpl::createSource()
{
    CheckContext(this);

    SourceImpl *source;
    if(!mFreeSources.empty())
    {
        source = mFreeSources.back();
        mFreeSources.pop_back();
    }
    else
    {
        mAllSources.emplace_back(*this);
        source = &mAllSources.back();
    }
    return Source(source);
}


void ContextImpl::addPendingSource(SourceImpl *source, SharedFuture<Buffer> future)
{
    auto iter = std::lower_bound(mPendingSources.begin(), mPendingSources.end(), source,
        [](const PendingSource &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter != mPendingSources.end() && iter->mSource == source)
        iter->mFuture = std::move(future);
    else
        mPendingSources.insert(iter, {source, std::move(future)});
}

void ContextImpl::removePendingSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mPendingSources.begin(), mPendingSources.end(), source,
        [](const PendingSource &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter != mPendingSources.end() && iter->mSource == source)
        mPendingSources.erase(iter);
}

bool ContextImpl::isPendingSource(const SourceImpl *source) const
{
    auto iter = std::lower_bound(mPendingSources.begin(), mPendingSources.end(), source,
        [](const PendingSource &lhs, const SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    return (iter != mPendingSources.end() && iter->mSource == source);
}

void ContextImpl::addFadingSource(SourceImpl *source, std::chrono::nanoseconds duration, ALfloat gain)
{
    auto iter = std::lower_bound(mFadingSources.begin(), mFadingSources.end(), source,
        [](const SourceFadeUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter == mFadingSources.end() || iter->mSource != source)
    {
        auto now = mDevice.getClockTime();
        mFadingSources.emplace(iter, SourceFadeUpdateEntry{source, now, now+duration, true, gain});
    }
}

void ContextImpl::removeFadingSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mFadingSources.begin(), mFadingSources.end(), source,
        [](const SourceFadeUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter != mFadingSources.end() && iter->mSource == source)
        mFadingSources.erase(iter);
}

void ContextImpl::addPlayingSource(SourceImpl *source, ALuint id)
{
    auto iter = std::lower_bound(mPlaySources.begin(), mPlaySources.end(), source,
        [](const SourceBufferUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter == mPlaySources.end() || iter->mSource != source)
        mPlaySources.insert(iter, {source,id});
}

void ContextImpl::addPlayingSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mStreamSources.begin(), mStreamSources.end(), source,
        [](const SourceStreamUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter == mStreamSources.end() || iter->mSource != source)
        mStreamSources.insert(iter, {source});
}

void ContextImpl::removePlayingSource(SourceImpl *source)
{
    auto iter0 = std::lower_bound(mPlaySources.begin(), mPlaySources.end(), source,
        [](const SourceBufferUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter0 != mPlaySources.end() && iter0->mSource == source)
        mPlaySources.erase(iter0);
    else
    {
        auto iter1 = std::lower_bound(mStreamSources.begin(), mStreamSources.end(), source,
            [](const SourceStreamUpdateEntry &lhs, SourceImpl *rhs) -> bool
            { return lhs.mSource < rhs; }
        );
        if(iter1 != mStreamSources.end() && iter1->mSource == source)
            mStreamSources.erase(iter1);
    }
}


void ContextImpl::addStream(SourceImpl *source)
{
    std::lock_guard<std::mutex> lock(mSourceStreamMutex);
    if(mThread.get_id() == std::thread::id())
        mThread = std::thread(std::mem_fn(&ContextImpl::backgroundProc), this);
    auto iter = std::lower_bound(mStreamingSources.begin(), mStreamingSources.end(), source);
    if(iter == mStreamingSources.end() || *iter != source)
        mStreamingSources.insert(iter, source);
}

void ContextImpl::removeStream(SourceImpl *source)
{
    std::lock_guard<std::mutex> lock(mSourceStreamMutex);
    auto iter = std::lower_bound(mStreamingSources.begin(), mStreamingSources.end(), source);
    if(iter != mStreamingSources.end() && *iter == source)
        mStreamingSources.erase(iter);
}

void ContextImpl::removeStreamNoLock(SourceImpl *source)
{
    auto iter = std::lower_bound(mStreamingSources.begin(), mStreamingSources.end(), source);
    if(iter != mStreamingSources.end() && *iter == source)
        mStreamingSources.erase(iter);
}


DECL_THUNK0(AuxiliaryEffectSlot, Context, createAuxiliaryEffectSlot,)
AuxiliaryEffectSlot ContextImpl::createAuxiliaryEffectSlot()
{
    if(!hasExtension(AL::EXT_EFX))
        throw std::runtime_error("AuxiliaryEffectSlots not supported");
    CheckContext(this);

    auto slot = MakeUnique<AuxiliaryEffectSlotImpl>(*this);
    auto iter = std::lower_bound(mEffectSlots.begin(), mEffectSlots.end(), slot);
    return AuxiliaryEffectSlot(mEffectSlots.insert(iter, std::move(slot))->get());
}

void ContextImpl::freeEffectSlot(AuxiliaryEffectSlotImpl *slot)
{
    auto iter = std::lower_bound(mEffectSlots.begin(), mEffectSlots.end(), slot,
        [](const UniquePtr<AuxiliaryEffectSlotImpl> &lhs, AuxiliaryEffectSlotImpl *rhs) -> bool
        { return lhs.get() < rhs; }
    );
    if(iter != mEffectSlots.end() && iter->get() == slot)
        mEffectSlots.erase(iter);
}


DECL_THUNK0(Effect, Context, createEffect,)
Effect ContextImpl::createEffect()
{
    if(!hasExtension(AL::EXT_EFX))
        throw std::runtime_error("Effects not supported");
    CheckContext(this);

    auto effect = MakeUnique<EffectImpl>(*this);
    auto iter = std::lower_bound(mEffects.begin(), mEffects.end(), effect);
    return Effect(mEffects.insert(iter, std::move(effect))->get());
}

void ContextImpl::freeEffect(EffectImpl *effect)
{
    auto iter = std::lower_bound(mEffects.begin(), mEffects.end(), effect,
        [](const UniquePtr<EffectImpl> &lhs, EffectImpl *rhs) -> bool
        { return lhs.get() < rhs; }
    );
    if(iter != mEffects.end() && iter->get() == effect)
        mEffects.erase(iter);
}


DECL_THUNK0(SourceGroup, Context, createSourceGroup,)
SourceGroup ContextImpl::createSourceGroup()
{
    auto srcgroup = MakeUnique<SourceGroupImpl>(*this);
    auto iter = std::lower_bound(mSourceGroups.begin(), mSourceGroups.end(), srcgroup);

    iter = mSourceGroups.insert(iter, std::move(srcgroup));
    return SourceGroup(iter->get());
}

void ContextImpl::freeSourceGroup(SourceGroupImpl *group)
{
    auto iter = std::lower_bound(mSourceGroups.begin(), mSourceGroups.end(), group,
        [](const UniquePtr<SourceGroupImpl> &lhs, SourceGroupImpl *rhs) -> bool
        { return lhs.get() < rhs; }
    );
    if(iter != mSourceGroups.end() && iter->get() == group)
        mSourceGroups.erase(iter);
}


DECL_THUNK1(void, Context, setDopplerFactor,, ALfloat)
void ContextImpl::setDopplerFactor(ALfloat factor)
{
    if(!(factor >= 0.0f))
        throw std::domain_error("Doppler factor out of range");
    CheckContext(this);
    alDopplerFactor(factor);
}


DECL_THUNK1(void, Context, setSpeedOfSound,, ALfloat)
void ContextImpl::setSpeedOfSound(ALfloat speed)
{
    if(!(speed > 0.0f))
        throw std::domain_error("Speed of sound out of range");
    CheckContext(this);
    alSpeedOfSound(speed);
}


DECL_THUNK1(void, Context, setDistanceModel,, DistanceModel)
void ContextImpl::setDistanceModel(DistanceModel model)
{
    CheckContext(this);
    alDistanceModel((ALenum)model);
}


DECL_THUNK0(void, Context, update,)
void ContextImpl::update()
{
    CheckContext(this);
    mPendingSources.erase(
        std::remove_if(mPendingSources.begin(), mPendingSources.end(),
            [](PendingSource &entry) -> bool
            { return !entry.mSource->checkPending(entry.mFuture); }
        ), mPendingSources.end()
    );
    if(!mFadingSources.empty())
    {
        auto cur_time = mDevice.getClockTime();
        mFadingSources.erase(
            std::remove_if(mFadingSources.begin(), mFadingSources.end(),
                [cur_time](SourceFadeUpdateEntry &entry) -> bool
                { return !entry.mSource->fadeUpdate(cur_time, entry); }
            ), mFadingSources.end()
        );
    }
    mPlaySources.erase(
        std::remove_if(mPlaySources.begin(), mPlaySources.end(),
            [](const SourceBufferUpdateEntry &entry) -> bool
            { return !entry.mSource->playUpdate(entry.mId); }
        ), mPlaySources.end()
    );
    mStreamSources.erase(
        std::remove_if(mStreamSources.begin(), mStreamSources.end(),
            [](const SourceStreamUpdateEntry &entry) -> bool
            { return !entry.mSource->playUpdate(); }
        ), mStreamSources.end()
    );

    if(!mWakeInterval.load(std::memory_order_relaxed).count())
    {
        // For performance reasons, don't wait for the thread's mutex. This
        // should be called often enough to keep up with any and all streams
        // regardless.
        mWakeThread.notify_all();
    }

    if(hasExtension(AL::EXT_disconnect) && mIsConnected)
    {
        ALCint connected;
        alcGetIntegerv(mDevice.getALCdevice(), ALC_CONNECTED, 1, &connected);
        mIsConnected = static_cast<bool>(connected);
        if(!mIsConnected && mMessage.get()) mMessage->deviceDisconnected(Device(&mDevice));
    }
}

DECL_THUNK0(Device, Context, getDevice,)
DECL_THUNK0(std::chrono::milliseconds, Context, getAsyncWakeInterval, const)
DECL_THUNK0(Listener, Context, getListener,)
DECL_THUNK0(SharedPtr<MessageHandler>, Context, getMessageHandler, const)

void Context::MakeCurrent(Context context)
{ ContextImpl::MakeCurrent(context.pImpl); }

Context Context::GetCurrent()
{ return Context(ContextImpl::GetCurrent()); }

void Context::MakeThreadCurrent(Context context)
{ ContextImpl::MakeThreadCurrent(context.pImpl); }

Context Context::GetThreadCurrent()
{ return Context(ContextImpl::GetThreadCurrent()); }


DECL_THUNK1(void, Listener, setGain,, ALfloat)
void ListenerImpl::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::domain_error("Gain out of range");
    CheckContext(mContext);
    alListenerf(AL_GAIN, gain);
}


DECL_THUNK3(void, Listener, set3DParameters,, const Vector3&, const Vector3&, const Vector3Pair&)
void ListenerImpl::set3DParameters(const Vector3 &position, const Vector3 &velocity, const std::pair<Vector3,Vector3> &orientation)
{
    static_assert(sizeof(orientation) == sizeof(ALfloat[6]), "Invalid Vector3 pair size");
    CheckContext(mContext);
    Batcher batcher = mContext->getBatcher();
    alListenerfv(AL_POSITION, position.getPtr());
    alListenerfv(AL_VELOCITY, velocity.getPtr());
    alListenerfv(AL_ORIENTATION, orientation.first.getPtr());
}

DECL_THUNK1(void, Listener, setPosition,, const Vector3&)
void ListenerImpl::setPosition(const Vector3 &position)
{
    CheckContext(mContext);
    alListenerfv(AL_POSITION, position.getPtr());
}

DECL_THUNK1(void, Listener, setPosition,, const ALfloat*)
void ListenerImpl::setPosition(const ALfloat *pos)
{
    CheckContext(mContext);
    alListenerfv(AL_POSITION, pos);
}

DECL_THUNK1(void, Listener, setVelocity,, const Vector3&)
void ListenerImpl::setVelocity(const Vector3 &velocity)
{
    CheckContext(mContext);
    alListenerfv(AL_VELOCITY, velocity.getPtr());
}

DECL_THUNK1(void, Listener, setVelocity,, const ALfloat*)
void ListenerImpl::setVelocity(const ALfloat *vel)
{
    CheckContext(mContext);
    alListenerfv(AL_VELOCITY, vel);
}

DECL_THUNK1(void, Listener, setOrientation,, const Vector3Pair&)
void ListenerImpl::setOrientation(const std::pair<Vector3,Vector3> &orientation)
{
    CheckContext(mContext);
    alListenerfv(AL_ORIENTATION, orientation.first.getPtr());
}

DECL_THUNK2(void, Listener, setOrientation,, const ALfloat*, const ALfloat*)
void ListenerImpl::setOrientation(const ALfloat *at, const ALfloat *up)
{
    CheckContext(mContext);
    ALfloat ori[6] = { at[0], at[1], at[2], up[0], up[1], up[2] };
    alListenerfv(AL_ORIENTATION, ori);
}

DECL_THUNK1(void, Listener, setOrientation,, const ALfloat*)
void ListenerImpl::setOrientation(const ALfloat *ori)
{
    CheckContext(mContext);
    alListenerfv(AL_ORIENTATION, ori);
}

DECL_THUNK1(void, Listener, setMetersPerUnit,, ALfloat)
void ListenerImpl::setMetersPerUnit(ALfloat m_u)
{
    if(!(m_u > 0.0f))
        throw std::domain_error("Invalid meters per unit");
    CheckContext(mContext);
    if(mContext->hasExtension(AL::EXT_EFX))
        alListenerf(AL_METERS_PER_UNIT, m_u);
}

}
