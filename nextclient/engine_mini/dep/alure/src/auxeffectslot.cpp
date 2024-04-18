
#include "config.h"

#include "auxeffectslot.h"

#include <stdexcept>

#include "context.h"
#include "effect.h"

namespace alure {

static inline bool operator<(const SourceSend &lhs, const SourceSend &rhs)
{ return lhs.mSource < rhs.mSource || (lhs.mSource == rhs.mSource && lhs.mSend < rhs.mSend); }
static inline bool operator==(const SourceSend &lhs, const SourceSend &rhs)
{ return lhs.mSource == rhs.mSource && lhs.mSend == rhs.mSend; }
static inline bool operator!=(const SourceSend &lhs, const SourceSend &rhs)
{ return !(lhs == rhs); }


AuxiliaryEffectSlotImpl::AuxiliaryEffectSlotImpl(ContextImpl &context) : mContext(context)
{
    alGetError();
    mContext.alGenAuxiliaryEffectSlots(1, &mId);
    throw_al_error("Failed to create AuxiliaryEffectSlot");
}

AuxiliaryEffectSlotImpl::~AuxiliaryEffectSlotImpl()
{
    if(UNLIKELY(mId != 0) && alcGetCurrentContext() == mContext.getALCcontext())
    {
        mContext.alDeleteAuxiliaryEffectSlots(1, &mId);
        mId = 0;
    }
}

void AuxiliaryEffectSlotImpl::addSourceSend(SourceSend source_send)
{
    auto iter = std::lower_bound(mSourceSends.begin(), mSourceSends.end(), source_send);
    if(iter == mSourceSends.end() || *iter != source_send)
        mSourceSends.insert(iter, source_send);
}

void AuxiliaryEffectSlotImpl::removeSourceSend(SourceSend source_send)
{
    auto iter = std::lower_bound(mSourceSends.begin(), mSourceSends.end(), source_send);
    if(iter != mSourceSends.end() && *iter == source_send)
        mSourceSends.erase(iter);
}


DECL_THUNK1(void, AuxiliaryEffectSlot, setGain,, ALfloat)
void AuxiliaryEffectSlotImpl::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f && gain <= 1.0f))
        throw std::domain_error("Gain out of range");
    CheckContext(mContext);
    mContext.alAuxiliaryEffectSlotf(mId, AL_EFFECTSLOT_GAIN, gain);
}

DECL_THUNK1(void, AuxiliaryEffectSlot, setSendAuto,, bool)
void AuxiliaryEffectSlotImpl::setSendAuto(bool sendauto)
{
    CheckContext(mContext);
    mContext.alAuxiliaryEffectSloti(mId, AL_EFFECTSLOT_AUXILIARY_SEND_AUTO, sendauto ? AL_TRUE : AL_FALSE);
}

DECL_THUNK1(void, AuxiliaryEffectSlot, applyEffect,, Effect)
void AuxiliaryEffectSlotImpl::applyEffect(Effect effect)
{
    const EffectImpl *eff = effect.getHandle();
    if(eff) CheckContexts(mContext, eff->getContext());
    CheckContext(mContext);

    mContext.alAuxiliaryEffectSloti(mId,
        AL_EFFECTSLOT_EFFECT, eff ? eff->getId() : AL_EFFECT_NULL
    );
}


void AuxiliaryEffectSlot::destroy()
{
    AuxiliaryEffectSlotImpl *i = pImpl;
    pImpl = nullptr;
    i->destroy();
}
void AuxiliaryEffectSlotImpl::destroy()
{
    CheckContext(mContext);

    if(!mSourceSends.empty())
    {
        Vector<SourceSend> source_sends;
        source_sends.swap(mSourceSends);

        auto batcher = mContext.getBatcher();
        for(const SourceSend &srcsend : source_sends)
            srcsend.mSource.getHandle()->setAuxiliarySend(nullptr, srcsend.mSend);
    }

    alGetError();
    mContext.alDeleteAuxiliaryEffectSlots(1, &mId);
    throw_al_error("AuxiliaryEffectSlot failed to delete");
    mId = 0;

    mContext.freeEffectSlot(this);
}

DECL_THUNK0(Vector<SourceSend>, AuxiliaryEffectSlot, getSourceSends, const)
DECL_THUNK0(size_t, AuxiliaryEffectSlot, getUseCount, const)

} // namespace alure
