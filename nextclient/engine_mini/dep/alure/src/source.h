#ifndef SOURCE_H
#define SOURCE_H

#include "main.h"

#include <atomic>
#include <mutex>

namespace alure {

class ALBufferStream;

struct SendProps {
    ALuint mSendIdx;
    AuxiliaryEffectSlotImpl *mSlot{nullptr};
    ALuint mFilter{AL_FILTER_NULL};

    SendProps(ALuint send, AuxiliaryEffectSlotImpl *slot) : mSendIdx(send), mSlot(slot)
    { }
    SendProps(ALuint send, ALuint filter) : mSendIdx(send), mFilter(filter)
    { }
    SendProps(ALuint send, AuxiliaryEffectSlotImpl *slot, ALuint filter)
      : mSendIdx(send), mSlot(slot), mFilter(filter)
    { }
};

struct SourceBufferUpdateEntry {
    SourceImpl *mSource;
    ALuint mId;
};
struct SourceStreamUpdateEntry {
    SourceImpl *mSource;
};

struct SourceFadeUpdateEntry {
    SourceImpl *mSource;

    std::chrono::nanoseconds mFadeTimeStart;
    std::chrono::nanoseconds mFadeTimeTarget;
    bool mIsFadeOut;
    ALfloat mFadeGainMult;
};


class SourceImpl {
    ContextImpl &mContext;
    ALuint mId;

    BufferImpl *mBuffer;
    UniquePtr<ALBufferStream> mStream;

    SourceGroupImpl *mGroup;
    ALfloat mGroupPitch;
    ALfloat mGroupGain;

    ALfloat mFadeGain;

    mutable std::mutex mMutex;
    std::atomic<bool> mIsAsync;

    std::atomic<bool> mPaused;
    uint64_t mOffset;
    ALfloat mPitch;
    ALfloat mGain;
    ALfloat mMinGain, mMaxGain;
    ALfloat mRefDist, mMaxDist;
    Vector3 mPosition;
    Vector3 mVelocity;
    Vector3 mDirection;
    Vector3 mOrientation[2];
    ALfloat mConeInnerAngle, mConeOuterAngle;
    ALfloat mConeOuterGain, mConeOuterGainHF;
    ALfloat mRolloffFactor, mRoomRolloffFactor;
    ALfloat mDopplerFactor;
    ALfloat mAirAbsorptionFactor;
    ALfloat mRadius;
    ALfloat mStereoAngles[2];
    Spatialize mSpatialize;
    ALsizei mResampler;
    bool mLooping : 1;
    bool mRelative : 1;
    bool mDryGainHFAuto : 1;
    bool mWetGainAuto : 1;
    bool mWetGainHFAuto : 1;

    ALuint mDirectFilter;
    Vector<SendProps> mEffectSlots;

    ALuint mPriority;

    void resetProperties();
    void applyProperties(bool looping) const;

    ALint refillBufferStream();

    void setFilterParams(ALuint &filterid, const FilterParams &params);

public:
    SourceImpl(ContextImpl &context);
    ~SourceImpl();

    ALuint getId() const { return mId; }

    bool checkPending(SharedFuture<Buffer> &future);
    bool fadeUpdate(std::chrono::nanoseconds cur_fade_time, SourceFadeUpdateEntry &fade);
    bool playUpdate(ALuint id);
    bool playUpdate();
    bool updateAsync();

    void unsetGroup();
    void groupPropUpdate(ALfloat gain, ALfloat pitch);

    void checkPaused();
    void unsetPaused() { mPaused = false; }

    void play(Buffer buffer);
    void play(SharedPtr<Decoder>&& decoder, ALsizei chunk_len, ALsizei queue_size);
    void play(SharedFuture<Buffer>&& future_buffer);
    void stop();
    void makeStopped(bool dolock=true);
    void fadeOutToStop(ALfloat gain, std::chrono::milliseconds duration);
    void pause();
    void resume();

    bool isPending() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isPlayingOrPending() const;

    void setGroup(SourceGroup group);
    SourceGroup getGroup() const { return SourceGroup(mGroup); }

    void setPriority(ALuint priority);
    ALuint getPriority() const { return mPriority; }

    void setOffset(uint64_t offset);
    std::pair<uint64_t,std::chrono::nanoseconds> getSampleOffsetLatency() const;
    std::pair<Seconds,Seconds> getSecOffsetLatency() const;

    void setLooping(bool looping);
    bool getLooping() const { return mLooping; }

    void setPitch(ALfloat pitch);
    ALfloat getPitch() const { return mPitch; }

    void setGain(ALfloat gain);
    ALfloat getGain() const { return mGain; }

    void setGainRange(ALfloat mingain, ALfloat maxgain);
    std::pair<ALfloat,ALfloat> getGainRange() const
    { return {mMinGain, mMaxGain}; }

    void setDistanceRange(ALfloat refdist, ALfloat maxdist);
    std::pair<ALfloat,ALfloat> getDistanceRange() const
    { return {mRefDist, mMaxDist}; }

    void set3DParameters(const Vector3 &position, const Vector3 &velocity, const Vector3 &direction);
    void set3DParameters(const Vector3 &position, const Vector3 &velocity, const std::pair<Vector3,Vector3> &orientation);

    void setPosition(const Vector3 &position);
    void setPosition(const ALfloat *pos);
    Vector3 getPosition() const { return mPosition; }

    void setVelocity(const Vector3 &velocity);
    void setVelocity(const ALfloat *vel);
    Vector3 getVelocity() const { return mVelocity; }

    void setDirection(const Vector3 &direction);
    void setDirection(const ALfloat *dir);
    Vector3 getDirection() const { return mDirection; }

    void setOrientation(const std::pair<Vector3,Vector3> &orientation);
    void setOrientation(const ALfloat *at, const ALfloat *up);
    void setOrientation(const ALfloat *ori);
    std::pair<Vector3,Vector3> getOrientation() const
    { return {mOrientation[0], mOrientation[1]}; }

    void setConeAngles(ALfloat inner, ALfloat outer);
    std::pair<ALfloat,ALfloat> getConeAngles() const
    { return {mConeInnerAngle, mConeOuterAngle}; }

    void setOuterConeGains(ALfloat gain, ALfloat gainhf=1.0f);
    std::pair<ALfloat,ALfloat> getOuterConeGains() const
    { return {mConeOuterGain, mConeOuterGainHF}; }

    void setRolloffFactors(ALfloat factor, ALfloat roomfactor=0.0f);
    std::pair<ALfloat,ALfloat> getRolloffFactors() const
    { return {mRolloffFactor, mRoomRolloffFactor}; }

    void setDopplerFactor(ALfloat factor);
    ALfloat getDopplerFactor() const { return mDopplerFactor; }

    void setRelative(bool relative);
    bool getRelative() const { return mRelative; }

    void setRadius(ALfloat radius);
    ALfloat getRadius() const { return mRadius; }

    void setStereoAngles(ALfloat leftAngle, ALfloat rightAngle);
    std::pair<ALfloat,ALfloat> getStereoAngles() const
    { return std::make_pair(mStereoAngles[0], mStereoAngles[1]); }

    void set3DSpatialize(Spatialize spatialize);
    Spatialize get3DSpatialize() const { return mSpatialize; }

    void setResamplerIndex(ALsizei index);
    ALsizei getResamplerIndex() const { return mResampler; }

    void setAirAbsorptionFactor(ALfloat factor);
    ALfloat getAirAbsorptionFactor() const { return mAirAbsorptionFactor; }

    void setGainAuto(bool directhf, bool send, bool sendhf);
    std::tuple<bool,bool,bool> getGainAuto() const
    { return std::make_tuple(mDryGainHFAuto, mWetGainAuto, mWetGainHFAuto); }

    void setDirectFilter(const FilterParams &filter);
    void setSendFilter(ALuint send, const FilterParams &filter);
    void setAuxiliarySend(AuxiliaryEffectSlot slot, ALuint send);
    void setAuxiliarySendFilter(AuxiliaryEffectSlot slot, ALuint send, const FilterParams &filter);

    void destroy();
};

} // namespace alure

#endif /* SOURCE_H */

