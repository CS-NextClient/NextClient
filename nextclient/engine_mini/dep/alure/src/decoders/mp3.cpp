
#include "mp3.hpp"

#include <stdexcept>
#include <iostream>
#include <cassert>

#include "context.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3.h"

namespace {

constexpr size_t MinMp3DataSize = 16384;
constexpr size_t MaxMp3DataSize = MinMp3DataSize * 8;

size_t append_file_data(std::istream &file, alure::Vector<uint8_t> &data, size_t count)
{
    size_t old_size = data.size();
    if(old_size >= MaxMp3DataSize || count == 0)
        return 0;
    count = std::min(count, MaxMp3DataSize - old_size);
    data.resize(old_size + count);

    file.clear();
    file.read(reinterpret_cast<char*>(data.data()+old_size), count);
    size_t got = file.gcount();
    data.resize(old_size + got);

    return got;
}

size_t find_i3dv2(alure::ArrayView<uint8_t> data)
{
    if(data.size() > 10 && memcmp(data.data(), "ID3", 3) == 0)
        return (((data[6]&0x7f) << 21) | ((data[7]&0x7f) << 14) |
                ((data[8]&0x7f) <<  7) | ((data[9]&0x7f)      )) + 10;
    return 0;
}

int decode_frame(std::istream &file, mp3dec_t &mp3, alure::Vector<uint8_t> &file_data,
                 float *sample_data, mp3dec_frame_info_t *frame_info)
{
    if(file_data.size() < MinMp3DataSize && !file.eof())
    {
        size_t todo = MinMp3DataSize - file_data.size();
        append_file_data(file, file_data, todo);
    }

    int samples_to_get = mp3dec_decode_frame(&mp3, file_data.data(), file_data.size(),
                                             sample_data, frame_info);
    while(samples_to_get == 0 && !file.eof())
    {
        if(append_file_data(file, file_data, MinMp3DataSize) == 0)
            break;
        samples_to_get = mp3dec_decode_frame(&mp3, file_data.data(), file_data.size(),
                                             sample_data, frame_info);
    }
    return samples_to_get;
}

} // namespace


namespace alure {

class Mp3Decoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    Vector<uint8_t> mFileData;

    mp3dec_t mMp3;
    Vector<float> mSampleData;
    mp3dec_frame_info_t mLastFrame{};
    mutable std::mutex mMutex;

    mutable std::streamsize mSampleCount{-1};
    ChannelConfig mChannels{ChannelConfig::Mono};
    SampleType mSampleType{SampleType::UInt8};
    int mSampleRate{0};
    bool mHasLoopPts{false};
    std::pair<uint64_t, uint64_t> mLoopPts{0, 0};

public:
    Mp3Decoder(UniquePtr<std::istream> file, Vector<uint8_t>&& initial_data,
               const mp3dec_t &mp3, const mp3dec_frame_info_t &first_frame,
               ChannelConfig chans, SampleType stype, int srate) noexcept
      : mFile(std::move(file)), mFileData(std::move(initial_data)), mMp3(mp3)
      , mLastFrame(first_frame), mChannels(chans), mSampleType(stype), mSampleRate(srate)
    { }
    ~Mp3Decoder() override { }

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    bool hasLoopPoints() const noexcept override;
    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

ALuint Mp3Decoder::getFrequency() const noexcept { return mSampleRate; }
ChannelConfig Mp3Decoder::getChannelConfig() const noexcept { return mChannels; }
SampleType Mp3Decoder::getSampleType() const noexcept { return mSampleType; }

uint64_t Mp3Decoder::getLength() const noexcept
{
    if(LIKELY(mSampleCount >= 0))
        return mSampleCount;

    std::lock_guard<std::mutex> _(mMutex);

    mFile->clear();
    std::streamsize oldfpos = mFile->tellg();
    if(oldfpos < 0 || !mFile->seekg(0))
    {
        mSampleCount = 0;
        return mSampleCount;
    }

    Vector<uint8_t> file_data;
    mp3dec_t mp3;

    mp3dec_init(&mp3);

    append_file_data(*mFile, file_data, MinMp3DataSize);

    size_t id_size = find_i3dv2(file_data);
    if(id_size > 0)
    {
        if(id_size <= file_data.size())
            file_data.erase(file_data.begin(), file_data.begin()+id_size);
        else
        {
            mFile->ignore(id_size - file_data.size());
            file_data.clear();
        }
    }

    std::streamsize count = 0;
    do {
        // Read the next frame.
        mp3dec_frame_info_t frame_info{};
        int samples_to_get = decode_frame(*mFile, mp3, file_data, nullptr, &frame_info);
        if(samples_to_get <= 0) break;

        // Don't continue if the frame changed format
        if((mChannels == ChannelConfig::Mono   && frame_info.channels != 1) ||
           (mChannels == ChannelConfig::Stereo && frame_info.channels != 2) ||
           mSampleRate != frame_info.hz)
            break;

        // Keep going to the next frame
        if(file_data.size() >= (size_t)frame_info.frame_bytes)
            file_data.erase(file_data.begin(), file_data.begin()+frame_info.frame_bytes);
        else
        {
            mFile->ignore(frame_info.frame_bytes - file_data.size());
            file_data.clear();
        }
        count += samples_to_get;
    } while(1);
    mSampleCount = count;

    mFile->clear();
    mFile->seekg(oldfpos);
    return mSampleCount;
}

bool Mp3Decoder::seek(uint64_t pos) noexcept
{
    // Use temporary local storage to avoid trashing current data in case of
    // failure.
    Vector<uint8_t> file_data;
    mp3dec_t mp3;

    mp3dec_init(&mp3);

    // Seeking to somewhere in the file. Backup the current file position and
    // reset back to the beginning.
    // TODO: Obvious optimization: Track the current sample offset and don't
    // rewind if seeking forward.
    mFile->clear();
    std::streamsize oldfpos = mFile->tellg();
    if(oldfpos < 0 || !mFile->seekg(0))
        return false;

    append_file_data(*mFile, file_data, MinMp3DataSize);

    size_t id_size = find_i3dv2(file_data);
    if(id_size > 0)
    {
        if(id_size <= file_data.size())
            file_data.erase(file_data.begin(), file_data.begin()+id_size);
        else
        {
            mFile->ignore(id_size - file_data.size());
            file_data.clear();
        }
    }

    uint64_t curpos = 0;
    do {
        // Read the next frame.
        mp3dec_frame_info_t frame_info{};
        int samples_to_get = decode_frame(*mFile, mp3, file_data, nullptr, &frame_info);
        if(samples_to_get <= 0) break;

        // Don't continue if the frame changed format
        if((mChannels == ChannelConfig::Mono   && frame_info.channels != 1) ||
           (mChannels == ChannelConfig::Stereo && frame_info.channels != 2) ||
           mSampleRate != frame_info.hz)
            break;

        if((uint64_t)samples_to_get > pos - curpos)
        {
            // Desired sample is within this frame, decode the samples and go
            // to the desired offset.
            Vector<float> sample_data(MINIMP3_MAX_SAMPLES_PER_FRAME);
            samples_to_get = decode_frame(*mFile, mp3, file_data, sample_data.data(), &frame_info);

            if((uint64_t)samples_to_get > pos - curpos)
            {
                sample_data.resize(samples_to_get * frame_info.channels);
                sample_data.erase(sample_data.begin(),
                                  sample_data.begin() + (pos-curpos)*frame_info.channels);
                file_data.erase(file_data.begin(), file_data.begin()+frame_info.frame_bytes);
                mSampleData = std::move(sample_data);
                mFileData = std::move(file_data);
                mLastFrame = frame_info;
                mMp3 = mp3;
                return true;
            }
        }

        // Keep going to the next frame
        if(file_data.size() >= (size_t)frame_info.frame_bytes)
            file_data.erase(file_data.begin(), file_data.begin()+frame_info.frame_bytes);
        else
        {
            mFile->ignore(frame_info.frame_bytes - file_data.size());
            file_data.clear();
        }
        curpos += samples_to_get;
    } while(1);

    // Seeking failed. Restore original file position.
    mFile->clear();
    mFile->seekg(oldfpos);
    return false;
}

bool Mp3Decoder::hasLoopPoints() const noexcept { return mHasLoopPts; }

std::pair<uint64_t,uint64_t> Mp3Decoder::getLoopPoints() const noexcept
{
    return {0, std::numeric_limits<uint64_t>::max()};
}

ALuint Mp3Decoder::read(ALvoid *ptr, ALuint count) noexcept
{
    union {
        void *v;
        float *f;
        short *s;
    } dst = { ptr };
    ALuint total = 0;

    std::lock_guard<std::mutex> _(mMutex);
    while(total < count)
    {
        ALuint todo = count-total;

        if(!mSampleData.empty())
        {
            // Write out whatever samples we have.
            todo = std::min<ALuint>(todo, mSampleData.size()/mLastFrame.channels);

            size_t numspl = todo*mLastFrame.channels;
            if(mSampleType == SampleType::Float32)
            {
                std::copy(mSampleData.begin(), mSampleData.begin()+numspl, dst.f);
                dst.f += numspl;
            }
            else
            {
                mp3dec_f32_to_s16(mSampleData.data(), dst.s, numspl);
                dst.s += numspl;
            }
            mSampleData.erase(mSampleData.begin(), mSampleData.begin()+numspl);

            total += todo;
            continue;
        }

        // Read directly into the output buffer if it doesn't need conversion
        // and there's enough guaranteed room.
        float *samples_ptr;
        if(mSampleType == SampleType::Float32 &&
           todo*mLastFrame.channels >= MINIMP3_MAX_SAMPLES_PER_FRAME)
            samples_ptr = dst.f;
        else
        {
            mSampleData.resize(MINIMP3_MAX_SAMPLES_PER_FRAME);
            samples_ptr = mSampleData.data();
        }

        mp3dec_frame_info_t frame_info{};
        int samples_to_get = decode_frame(*mFile, mMp3, mFileData, samples_ptr, &frame_info);
        if(samples_to_get <= 0)
        {
            mSampleData.clear();
            break;
        }

        // Format changing not supported. End the stream.
        if((mChannels == ChannelConfig::Mono   && frame_info.channels != 1) ||
           (mChannels == ChannelConfig::Stereo && frame_info.channels != 2) ||
           mSampleRate != frame_info.hz)
        {
            mSampleData.clear();
            break;
        }

        // Remove used file data, update sample storage size with what we got
        mFileData.erase(mFileData.begin(), mFileData.begin()+frame_info.frame_bytes);
        mLastFrame = frame_info;
        if(!mSampleData.empty())
            mSampleData.resize(samples_to_get * frame_info.channels);
        else
        {
            dst.f += samples_to_get * frame_info.channels;
            total += samples_to_get;
        }
    }

    return total;
}


Mp3DecoderFactory::Mp3DecoderFactory() noexcept
{
}

Mp3DecoderFactory::~Mp3DecoderFactory()
{
}

SharedPtr<Decoder> Mp3DecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    Vector<uint8_t> initial_data;
    mp3dec_t mp3{};

    mp3dec_init(&mp3);

    // Make sure the file is valid and we get some samples.
    if(append_file_data(*file, initial_data, MinMp3DataSize) == 0)
        return nullptr;

    // If the file contains an ID3v2 tag, skip it.
    // TODO: Read it? Does it have e.g. sample length or loop points?
    size_t id_size = find_i3dv2(initial_data);
    if(id_size > 0)
    {
        if(id_size <= initial_data.size())
            initial_data.erase(initial_data.begin(), initial_data.begin()+id_size);
        else
        {
            file->ignore(id_size - initial_data.size());
            initial_data.clear();
        }
    }

    mp3dec_frame_info_t frame_info{};
    int samples_to_get = decode_frame(*file, mp3, initial_data, nullptr, &frame_info);
    if(!samples_to_get) return nullptr;

    if(frame_info.hz < 1)
        return nullptr;

    ChannelConfig chans = ChannelConfig::Mono;
    if(frame_info.channels == 1)
        chans = ChannelConfig::Mono;
    else if(frame_info.channels == 2)
        chans = ChannelConfig::Stereo;
    else
        return nullptr;

    SampleType stype = SampleType::Int16;
    if(ContextImpl::GetCurrent()->isSupported(chans, SampleType::Float32))
        stype = SampleType::Float32;

    return MakeShared<Mp3Decoder>(std::move(file), std::move(initial_data), mp3,
                                  frame_info, chans, stype, frame_info.hz);
}

} // namespace alure
