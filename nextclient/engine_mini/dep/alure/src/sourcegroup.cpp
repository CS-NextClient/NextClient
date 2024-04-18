
#include "sourcegroup.h"

#include <algorithm>

#include "source.h"
#include "context.h"

namespace alure {

void SourceGroupImpl::insertSubGroup(SourceGroupImpl *group)
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter == mSubGroups.end() || *iter != group)
        mSubGroups.insert(iter, group);
}

void SourceGroupImpl::eraseSubGroup(SourceGroupImpl *group)
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter != mSubGroups.end() && *iter == group) mSubGroups.erase(iter);
}


void SourceGroupImpl::unsetParent()
{
    mParent = nullptr;
    update(1.0f, 1.0f);
}

void SourceGroupImpl::update(ALfloat gain, ALfloat pitch)
{
    mParentProps.mGain = gain;
    mParentProps.mPitch = pitch;

    gain *= mGain;
    pitch *= mPitch;
    for(SourceImpl *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(SourceGroupImpl *group : mSubGroups)
        group->update(gain, pitch);
}


bool SourceGroupImpl::findInSubGroups(SourceGroupImpl *group) const
{
    auto iter = std::lower_bound(mSubGroups.begin(), mSubGroups.end(), group);
    if(iter != mSubGroups.end() && *iter == group) return true;

    for(SourceGroupImpl *grp : mSubGroups)
    {
        if(grp->findInSubGroups(group))
            return true;
    }
    return false;
}


void SourceGroupImpl::insertSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mSources.begin(), mSources.end(), source);
    if(iter == mSources.end() || *iter != source)
        mSources.insert(iter, source);
}

void SourceGroupImpl::eraseSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mSources.begin(), mSources.end(), source);
    if(iter != mSources.end() && *iter == source)
        mSources.erase(iter);
}


DECL_THUNK1(void, SourceGroup, setParentGroup,, SourceGroup)
void SourceGroupImpl::setParentGroup(SourceGroup group)
{
    CheckContext(mContext);

    SourceGroupImpl *parent = group.getHandle();
    if(!parent)
    {
        if(mParent)
            mParent->eraseSubGroup(this);
        mParent = nullptr;
        update(1.0f, 1.0f);
    }
    else
    {
        if(this == parent || findInSubGroups(parent))
            throw std::runtime_error("Attempted circular group chain");

        parent->insertSubGroup(this);

        Batcher batcher = mContext.getBatcher();
        if(mParent)
            mParent->eraseSubGroup(this);
        mParent = parent;
        update(mParent->getAppliedGain(), mParent->getAppliedPitch());
    }
}


DECL_THUNK0(Vector<Source>, SourceGroup, getSources, const)
Vector<Source> SourceGroupImpl::getSources() const
{
    Vector<Source> ret;
    ret.reserve(mSources.size());
    for(SourceImpl *src : mSources)
        ret.emplace_back(src);
    return ret;
}

DECL_THUNK0(Vector<SourceGroup>, SourceGroup, getSubGroups, const)
Vector<SourceGroup> SourceGroupImpl::getSubGroups() const
{
    Vector<SourceGroup> ret;
    ret.reserve(mSubGroups.size());
    for(SourceGroupImpl *grp : mSubGroups)
        ret.emplace_back(grp);
    return ret;
}


DECL_THUNK1(void, SourceGroup, setGain,, ALfloat)
void SourceGroupImpl::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::domain_error("Gain out of range");
    CheckContext(mContext);
    mGain = gain;
    gain *= mParentProps.mGain;
    ALfloat pitch = mPitch * mParentProps.mPitch;
    Batcher batcher = mContext.getBatcher();
    for(SourceImpl *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(SourceGroupImpl *group : mSubGroups)
        group->update(gain, pitch);
}

DECL_THUNK1(void, SourceGroup, setPitch,, ALfloat)
void SourceGroupImpl::setPitch(ALfloat pitch)
{
    if(!(pitch > 0.0f))
        throw std::domain_error("Pitch out of range");
    CheckContext(mContext);
    mPitch = pitch;
    ALfloat gain = mGain * mParentProps.mGain;
    pitch *= mParentProps.mPitch;
    Batcher batcher = mContext.getBatcher();
    for(SourceImpl *alsrc : mSources)
        alsrc->groupPropUpdate(gain, pitch);
    for(SourceGroupImpl *group : mSubGroups)
        group->update(gain, pitch);
}


void SourceGroupImpl::collectPlayingSourceIds(Vector<ALuint> &sourceids) const
{
    for(SourceImpl *alsrc : mSources)
    {
        if(ALuint id = alsrc->getId())
            sourceids.push_back(id);
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->collectPlayingSourceIds(sourceids);
}

void SourceGroupImpl::updatePausedStatus() const
{
    for(SourceImpl *alsrc : mSources)
        alsrc->checkPaused();
    for(SourceGroupImpl *group : mSubGroups)
        group->updatePausedStatus();
}

DECL_THUNK0(void, SourceGroup, pauseAll, const)
void SourceGroupImpl::pauseAll() const
{
    CheckContext(mContext);
    auto lock = mContext.getSourceStreamLock();

    Vector<ALuint> sourceids;
    sourceids.reserve(16);
    collectPlayingSourceIds(sourceids);
    if(!sourceids.empty())
    {
        alSourcePausev(static_cast<ALsizei>(sourceids.size()), sourceids.data());
        updatePausedStatus();
    }
    lock.unlock();
}


void SourceGroupImpl::collectPausedSourceIds(Vector<ALuint> &sourceids) const
{
    for(SourceImpl *alsrc : mSources)
    {
        if(alsrc->isPaused())
            sourceids.push_back(alsrc->getId());
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->collectPausedSourceIds(sourceids);
}

void SourceGroupImpl::updatePlayingStatus() const
{
    for(SourceImpl *alsrc : mSources)
        alsrc->unsetPaused();
    for(SourceGroupImpl *group : mSubGroups)
        group->updatePlayingStatus();
}

DECL_THUNK0(void, SourceGroup, resumeAll, const)
void SourceGroupImpl::resumeAll() const
{
    CheckContext(mContext);
    auto lock = mContext.getSourceStreamLock();

    Vector<ALuint> sourceids;
    sourceids.reserve(16);
    collectPausedSourceIds(sourceids);
    if(!sourceids.empty())
    {
        alSourcePlayv(static_cast<ALsizei>(sourceids.size()), sourceids.data());
        updatePlayingStatus();
    }
    lock.unlock();
}


void SourceGroupImpl::collectSourceIds(Vector<ALuint> &sourceids) const
{
    for(SourceImpl *alsrc : mSources)
    {
        if(ALuint id = alsrc->getId())
            sourceids.push_back(id);
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->collectSourceIds(sourceids);
}

void SourceGroupImpl::updateStoppedStatus() const
{
    for(SourceImpl *alsrc : mSources)
    {
        mContext.removePendingSource(alsrc);
        mContext.removeFadingSource(alsrc);
        mContext.removePlayingSource(alsrc);
        alsrc->makeStopped(false);
        mContext.send(&MessageHandler::sourceForceStopped, alsrc);
    }
    for(SourceGroupImpl *group : mSubGroups)
        group->updateStoppedStatus();
}

DECL_THUNK0(void, SourceGroup, stopAll, const)
void SourceGroupImpl::stopAll() const
{
    CheckContext(mContext);

    Vector<ALuint> sourceids;
    sourceids.reserve(16);
    collectSourceIds(sourceids);
    if(!sourceids.empty())
    {
        auto lock = mContext.getSourceStreamLock();
        alSourceRewindv(static_cast<ALsizei>(sourceids.size()), sourceids.data());
        updateStoppedStatus();
    }
}


void SourceGroup::destroy()
{
    SourceGroupImpl *i = pImpl;
    pImpl = nullptr;
    i->destroy();
}
void SourceGroupImpl::destroy()
{
    CheckContext(mContext);
    Batcher batcher = mContext.getBatcher();
    for(SourceImpl *source : mSources)
        source->unsetGroup();
    mSources.clear();
    for(SourceGroupImpl *group : mSubGroups)
        group->unsetParent();
    mSubGroups.clear();
    if(mParent)
        mParent->eraseSubGroup(this);
    mParent = nullptr;

    mContext.freeSourceGroup(this);
}


DECL_THUNK0(SourceGroup, SourceGroup, getParentGroup, const)
DECL_THUNK0(ALfloat, SourceGroup, getGain, const)
DECL_THUNK0(ALfloat, SourceGroup, getPitch, const)

} // namespace alure
