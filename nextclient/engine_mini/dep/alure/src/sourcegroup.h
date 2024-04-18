#ifndef SOURCEGROUP_H
#define SOURCEGROUP_H

#include "main.h"


namespace alure
{

struct SourceGroupProps {
    ALfloat mGain;
    ALfloat mPitch;

    SourceGroupProps() : mGain(1.0f), mPitch(1.0f) { }
};

class SourceGroupImpl : SourceGroupProps {
    ContextImpl &mContext;

    Vector<SourceImpl*> mSources;
    Vector<SourceGroupImpl*> mSubGroups;

    SourceGroupProps mParentProps;
    SourceGroupImpl *mParent;

    void update(ALfloat gain, ALfloat pitch);

    void unsetParent();

    void insertSubGroup(SourceGroupImpl *group);
    void eraseSubGroup(SourceGroupImpl *group);

    bool findInSubGroups(SourceGroupImpl *group) const;

    void collectPlayingSourceIds(Vector<ALuint> &sourceids) const;
    void updatePausedStatus() const;

    void collectPausedSourceIds(Vector<ALuint> &sourceids) const;
    void updatePlayingStatus() const;

    void collectSourceIds(Vector<ALuint> &sourceids) const;
    void updateStoppedStatus() const;

public:
    SourceGroupImpl(ContextImpl &context) : mContext(context), mParent(nullptr) { }

    ALfloat getAppliedGain() const { return mGain * mParentProps.mGain; }
    ALfloat getAppliedPitch() const { return mPitch * mParentProps.mPitch; }

    void insertSource(SourceImpl *source);
    void eraseSource(SourceImpl *source);

    void setParentGroup(SourceGroup group);
    SourceGroup getParentGroup() const { return SourceGroup(mParent); }

    Vector<Source> getSources() const;

    Vector<SourceGroup> getSubGroups() const;

    void setGain(ALfloat gain);
    ALfloat getGain() const { return mGain; }

    void setPitch(ALfloat pitch);
    ALfloat getPitch() const { return mPitch; }

    void pauseAll() const;
    void resumeAll() const;

    void stopAll() const;

    void destroy();
};

} // namespace alure2

#endif /* SOURCEGROUP_H */
