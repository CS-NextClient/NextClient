/*
 * An example showing how to use an external decoder to play files through the
 * DUMB library.
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <limits>
#include <thread>
#include <chrono>

#include "dumb.h"

#include "alure2.h"

namespace {

// Some I/O function callback wrappers for DUMB to read from an std::istream
#if DUMB_VERSION >= 2*10000
int cb_skip(void *user_data, dumb_off_t offset)
#else
int cb_skip(void *user_data, long offset)
#endif
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    if(stream->seekg(offset, std::ios_base::cur))
        return 0;
    return 1;
}

#if DUMB_VERSION >= 2*10000
dumb_ssize_t cb_read(char *ptr, size_t size, void *user_data)
#else
long cb_read(char *ptr, long size, void *user_data)
#endif
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    stream->read(ptr, size);
    return stream->gcount();
}

static int cb_read_char(void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    unsigned char ret;
    stream->read(reinterpret_cast<char*>(&ret), 1);
    if(stream->gcount() > 0) return ret;
    return -1;
}


// Inherit from alure::Decoder to make a custom decoder (DUMB for this example)
class DumbDecoder final : public alure::Decoder {
    alure::UniquePtr<std::istream> mFile;

    alure::UniquePtr<DUMBFILE_SYSTEM> mDfs;
    DUMBFILE *mDumbfile{nullptr};
    DUH *mDuh{nullptr};
    DUH_SIGRENDERER *mRenderer{nullptr};

    alure::SampleType mSampleType{alure::SampleType::UInt8};
    ALuint mFrequency{0};

    alure::Vector<sample_t> mSampleBuf;

public:
    DumbDecoder(alure::UniquePtr<std::istream> file, alure::UniquePtr<DUMBFILE_SYSTEM> dfs,
                DUMBFILE *dfile, DUH *duh, DUH_SIGRENDERER *renderer, alure::SampleType stype,
                ALuint srate) noexcept
      : mFile(std::move(file)), mDfs(std::move(dfs)), mDumbfile(dfile), mDuh(duh)
      , mRenderer(renderer), mSampleType(stype), mFrequency(srate)
    { }
    ~DumbDecoder() override
    {
        duh_end_sigrenderer(mRenderer);
        mRenderer = nullptr;

        unload_duh(mDuh);
        mDuh = nullptr;

        dumbfile_close(mDumbfile);
        mDumbfile = nullptr;
    }

    ALuint getFrequency() const noexcept override
    { return mFrequency; }
    alure::ChannelConfig getChannelConfig() const noexcept override
    {
        // We always have DUMB render to stereo
        return alure::ChannelConfig::Stereo;
    }
    alure::SampleType getSampleType() const noexcept override
    {
        // DUMB renders to 8.24 normalized fixed point, which we convert to
        // 32-bit float or signed 16-bit samples
        return mSampleType;
    }

    uint64_t getLength() const noexcept override
    {
        // Modules have no explicit length, they just keep playing as long as
        // more samples get generated.
        return 0;
    }

    bool seek(uint64_t) noexcept override
    {
        // Cannot seek
        return false;
    }

    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override
    {
        // No loop points
        return std::make_pair(0, 0);
    }

    ALuint read(ALvoid *ptr, ALuint count) noexcept override
    {
        ALuint ret = 0;

        mSampleBuf.resize(count*2);
        alure::Array<sample_t*,1> samples{{mSampleBuf.data()}};

        dumb_silence(samples[0], mSampleBuf.size());
        ret = duh_sigrenderer_generate_samples(mRenderer, 1.0f, 65536.0f/mFrequency, count,
                                               samples.data());
        if(mSampleType == alure::SampleType::Float32)
        {
            ALfloat *out = reinterpret_cast<ALfloat*>(ptr);
            for(ALuint i = 0;i < ret*2;i++)
                out[i] = (ALfloat)samples[0][i] * (1.0f/8388608.0f);
        }
        else
        {
            ALshort *out = reinterpret_cast<ALshort*>(ptr);
            for(ALuint i = 0;i < ret*2;i++)
            {
                sample_t smp = samples[0][i]>>8;
                if(smp < -32768) smp = -32768;
                else if(smp > 32767) smp = 32767;
                out[i] = smp;
            }
        }

        return ret;
    }
};

// Inherit from alure::DecoderFactory to use our custom decoder
class DumbFactory final : public alure::DecoderFactory {
    alure::SharedPtr<alure::Decoder> createDecoder(alure::UniquePtr<std::istream> &file) noexcept override
    {
        static const alure::Array<DUH*(*)(DUMBFILE*),3> init_funcs{{
            dumb_read_it, dumb_read_xm, dumb_read_s3m
        }};

        auto dfs = alure::MakeUnique<DUMBFILE_SYSTEM>();
        std::memset(dfs.get(), 0, sizeof(DUMBFILE_SYSTEM));
        dfs->open = nullptr;
        dfs->skip = cb_skip;
        dfs->getc = cb_read_char;
        dfs->getnc = cb_read;
        dfs->close = nullptr;

        alure::Context ctx = alure::Context::GetCurrent();
        alure::SampleType stype = alure::SampleType::Float32;
        if(!ctx.isSupported(alure::ChannelConfig::Stereo, stype))
            stype = alure::SampleType::Int16;
        ALuint freq = ctx.getDevice().getFrequency();

        DUMBFILE *dfile = nullptr;
        for(auto init : init_funcs)
        {
            dfile = dumbfile_open_ex(file.get(), dfs.get());
            if(!dfile) return nullptr;

            DUH *duh;
            if((duh=init(dfile)) != nullptr)
            {
                DUH_SIGRENDERER *renderer;
                if((renderer=duh_start_sigrenderer(duh, 0, 2, 0)) != nullptr)
                    return alure::MakeShared<DumbDecoder>(
                        std::move(file), std::move(dfs), dfile, duh, renderer, stype, freq
                    );

                unload_duh(duh);
                duh = nullptr;
            }

            dumbfile_close(dfile);
            dfile = nullptr;

            file->clear();
            if(!file->seekg(0))
                break;
        }

        return nullptr;
    }
};


// Helper class+method to print the time with human-readable formatting.
struct PrettyTime {
    alure::Seconds mTime;
};
inline std::ostream &operator<<(std::ostream &os, const PrettyTime &rhs)
{
    using hours = std::chrono::hours;
    using minutes = std::chrono::minutes;
    using seconds = std::chrono::seconds;
    using centiseconds = std::chrono::duration<int64_t, std::ratio<1, 100>>;
    using std::chrono::duration_cast;

    centiseconds t = duration_cast<centiseconds>(rhs.mTime);
    if(t.count() < 0)
    {
        os << '-';
        t *= -1;
    }

    // Only handle up to hour formatting
    if(t >= hours(1))
        os << duration_cast<hours>(t).count() << 'h' << std::setfill('0') << std::setw(2)
           << duration_cast<minutes>(t).count() << 'm';
    else
        os << duration_cast<minutes>(t).count() << 'm' << std::setfill('0');
    os << std::setw(2) << (duration_cast<seconds>(t).count() % 60) << '.' << std::setw(2)
       << (t.count() % 100) << 's' << std::setw(0) << std::setfill(' ');
    return os;
}

} // namespace


int main(int argc, char *argv[])
{
    alure::ArrayView<const char*> args(argv, argc);

    if(args.size() < 2)
    {
        std::cerr<< "Usage: "<<args.front()<<" [-device \"device name\"] files..." <<std::endl;
        return 1;
    }
    args = args.slice(1);

    // Set our custom factory for decoding modules.
    alure::RegisterDecoder("dumb", alure::MakeUnique<DumbFactory>());

    alure::DeviceManager devMgr = alure::DeviceManager::getInstance();

    alure::Device dev;
    if(args.size() > 2 && args[0] == alure::StringView("-device"))
    {
        dev = devMgr.openPlayback(args[1], std::nothrow);
        if(!dev) std::cerr<< "Failed to open \""<<args[1]<<"\" - trying default" <<std::endl;
        args = args.slice(2);
    }
    if(!dev) dev = devMgr.openPlayback();
    std::cout<< "Opened \""<<dev.getName()<<"\"" <<std::endl;

    alure::Context ctx = dev.createContext();
    alure::Context::MakeCurrent(ctx);

    for(;!args.empty();args = args.slice(1))
    {
        alure::SharedPtr<alure::Decoder> decoder = ctx.createDecoder(args.front());
        alure::Source source = ctx.createSource();

        source.play(decoder, 12000, 4);
        std::cout<< "Playing "<<args.front()<<" ("
                 << alure::GetSampleTypeName(decoder->getSampleType())<<", "
                 << alure::GetChannelConfigName(decoder->getChannelConfig())<<", "
                 << decoder->getFrequency()<<"hz)" <<std::endl;

        while(source.isPlaying())
        {
            std::cout<< "\r "<<PrettyTime{source.getSecOffset()};
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            ctx.update();
        }
        std::cout<<std::endl;

        source.destroy();
        decoder.reset();
    }

    alure::Context::MakeCurrent(nullptr);
    ctx.destroy();
    dev.close();

    return 0;
}
