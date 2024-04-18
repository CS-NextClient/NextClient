
#include "config.h"

#include "effect.h"

#include <stdexcept>

#include "context.h"


namespace {

template<typename T>
inline T clamp(const T& val, const T& min, const T& max)
{ return std::min<T>(std::max<T>(val, min), max); }

} // namespace

namespace alure {

EffectImpl::EffectImpl(ContextImpl &context) : mContext(context)
{
    alGetError();
    mContext.alGenEffects(1, &mId);
    throw_al_error("Failed to create Effect");
}

EffectImpl::~EffectImpl()
{
    if(UNLIKELY(mId != 0) && alcGetCurrentContext() == mContext.getALCcontext())
    {
        mContext.alDeleteEffects(1, &mId);
        mId = 0;
    }
}


DECL_THUNK1(void, Effect, setReverbProperties,, const EFXEAXREVERBPROPERTIES&)
void EffectImpl::setReverbProperties(const EFXEAXREVERBPROPERTIES &props)
{
    CheckContext(mContext);

    if(mType != AL_EFFECT_EAXREVERB && mType != AL_EFFECT_REVERB)
    {
        alGetError();
        mContext.alEffecti(mId, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        if(alGetError() == AL_NO_ERROR)
            mType = AL_EFFECT_EAXREVERB;
        else
        {
            mContext.alEffecti(mId, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
            throw_al_error("Failed to set reverb type");
            mType = AL_EFFECT_REVERB;
        }
    }

    if(mType == AL_EFFECT_EAXREVERB)
    {
#define SETPARAM(e,t,v) mContext.alEffectf((e), AL_EAXREVERB_##t, clamp((v), AL_EAXREVERB_MIN_##t, AL_EAXREVERB_MAX_##t))
        SETPARAM(mId, DENSITY, props.flDensity);
        SETPARAM(mId, DIFFUSION, props.flDiffusion);
        SETPARAM(mId, GAIN, props.flGain);
        SETPARAM(mId, GAINHF, props.flGainHF);
        SETPARAM(mId, GAINLF, props.flGainLF);
        SETPARAM(mId, DECAY_TIME, props.flDecayTime);
        SETPARAM(mId, DECAY_HFRATIO, props.flDecayHFRatio);
        SETPARAM(mId, DECAY_LFRATIO, props.flDecayLFRatio);
        SETPARAM(mId, REFLECTIONS_GAIN, props.flReflectionsGain);
        SETPARAM(mId, REFLECTIONS_DELAY, props.flReflectionsDelay);
        mContext.alEffectfv(mId, AL_EAXREVERB_REFLECTIONS_PAN, props.flReflectionsPan);
        SETPARAM(mId, LATE_REVERB_GAIN, props.flLateReverbGain);
        SETPARAM(mId, LATE_REVERB_DELAY, props.flLateReverbDelay);
        mContext.alEffectfv(mId, AL_EAXREVERB_LATE_REVERB_PAN, props.flLateReverbPan);
        SETPARAM(mId, ECHO_TIME, props.flEchoTime);
        SETPARAM(mId, ECHO_DEPTH, props.flEchoDepth);
        SETPARAM(mId, MODULATION_TIME, props.flModulationTime);
        SETPARAM(mId, MODULATION_DEPTH, props.flModulationDepth);
        SETPARAM(mId, AIR_ABSORPTION_GAINHF, props.flAirAbsorptionGainHF);
        SETPARAM(mId, HFREFERENCE, props.flHFReference);
        SETPARAM(mId, LFREFERENCE, props.flLFReference);
        SETPARAM(mId, ROOM_ROLLOFF_FACTOR, props.flRoomRolloffFactor);
        mContext.alEffecti(mId, AL_EAXREVERB_DECAY_HFLIMIT, (props.iDecayHFLimit ? AL_TRUE : AL_FALSE));
#undef SETPARAM
    }
    else if(mType == AL_EFFECT_REVERB)
    {
#define SETPARAM(e,t,v) mContext.alEffectf((e), AL_REVERB_##t, clamp((v), AL_REVERB_MIN_##t, AL_REVERB_MAX_##t))
        SETPARAM(mId, DENSITY, props.flDensity);
        SETPARAM(mId, DIFFUSION, props.flDiffusion);
        SETPARAM(mId, GAIN, props.flGain);
        SETPARAM(mId, GAINHF, props.flGainHF);
        SETPARAM(mId, DECAY_TIME, props.flDecayTime);
        SETPARAM(mId, DECAY_HFRATIO, props.flDecayHFRatio);
        SETPARAM(mId, REFLECTIONS_GAIN, props.flReflectionsGain);
        SETPARAM(mId, REFLECTIONS_DELAY, props.flReflectionsDelay);
        SETPARAM(mId, LATE_REVERB_GAIN, props.flLateReverbGain);
        SETPARAM(mId, LATE_REVERB_DELAY, props.flLateReverbDelay);
        SETPARAM(mId, AIR_ABSORPTION_GAINHF, props.flAirAbsorptionGainHF);
        SETPARAM(mId, ROOM_ROLLOFF_FACTOR, props.flRoomRolloffFactor);
        mContext.alEffecti(mId, AL_REVERB_DECAY_HFLIMIT, (props.iDecayHFLimit ? AL_TRUE : AL_FALSE));
#undef SETPARAM
    }
}

DECL_THUNK1(void, Effect, setChorusProperties,, const EFXCHORUSPROPERTIES&)
void EffectImpl::setChorusProperties(const EFXCHORUSPROPERTIES &props)
{
    CheckContext(mContext);

    if(mType != AL_EFFECT_CHORUS)
    {
        alGetError();
        mContext.alEffecti(mId, AL_EFFECT_TYPE, AL_EFFECT_CHORUS);
        throw_al_error("Failed to set chorus type");
        mType = AL_EFFECT_CHORUS;
    }

#define SETPARAM(t,v) AL_CHORUS_##t, clamp((v), AL_CHORUS_MIN_##t, AL_CHORUS_MAX_##t)
    mContext.alEffecti(mId, SETPARAM(WAVEFORM, props.iWaveform));
    mContext.alEffecti(mId, SETPARAM(PHASE, props.iPhase));
    mContext.alEffectf(mId, SETPARAM(RATE, props.flRate));
    mContext.alEffectf(mId, SETPARAM(DEPTH, props.flDepth));
    mContext.alEffectf(mId, SETPARAM(FEEDBACK, props.flFeedback));
    mContext.alEffectf(mId, SETPARAM(DELAY, props.flDelay));
#undef SETPARAM
}

void Effect::destroy()
{
    EffectImpl *i = pImpl;
    pImpl = nullptr;
    i->destroy();
}
void EffectImpl::destroy()
{
    CheckContext(mContext);

    alGetError();
    mContext.alDeleteEffects(1, &mId);
    throw_al_error("Effect failed to delete");
    mId = 0;

    mContext.freeEffect(this);
}

}
