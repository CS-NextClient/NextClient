#ifndef AUXEFFECTSLOT_H
#define AUXEFFECTSLOT_H

#include <algorithm>

#include "main.h"

namespace alure {

class AuxiliaryEffectSlotImpl {
    ContextImpl &mContext;
    ALuint mId{0};

    Vector<SourceSend> mSourceSends;

public:
    AuxiliaryEffectSlotImpl(ContextImpl &context);
    ~AuxiliaryEffectSlotImpl();

    void addSourceSend(SourceSend source_send);
    void removeSourceSend(SourceSend source_send);

    ContextImpl &getContext() { return mContext; }
    const ALuint &getId() const { return mId; }

    void setGain(ALfloat gain);
    void setSendAuto(bool sendauto);

    void applyEffect(Effect effect);

    void destroy();

    Vector<SourceSend> getSourceSends() const { return mSourceSends; }

    size_t getUseCount() const { return mSourceSends.size(); }
};

} // namespace alure

#endif /* AUXEFFECTSLOT_H */
