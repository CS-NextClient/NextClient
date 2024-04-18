#ifndef EFFECT_H
#define EFFECT_H

#include "main.h"

namespace alure {

class EffectImpl {
    ContextImpl &mContext;
    ALuint mId{0};
    ALenum mType{AL_NONE};

public:
    EffectImpl(ContextImpl &context);
    ~EffectImpl();

    void setReverbProperties(const EFXEAXREVERBPROPERTIES &props);
    void setChorusProperties(const EFXCHORUSPROPERTIES &props);

    void destroy();

    ContextImpl &getContext() const { return mContext; }
    ALuint getId() const { return mId; }
};

} // namespace alure

#endif /* EFFECT_H */
