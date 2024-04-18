#ifndef DEVICE_H
#define DEVICE_H

#include <mutex>

#include "main.h"


namespace alure {

enum class ALC {
    ENUMERATE_ALL_EXT,
    EXT_EFX,
    EXT_thread_local_context,
    SOFT_HRTF,
    SOFT_pause_device,

    EXTENSION_MAX
};

class DeviceImpl {
    ALCdevice *mDevice{nullptr};

    std::chrono::nanoseconds mTimeBase, mPauseTime;
    bool mIsPaused{false};

    Vector<UniquePtr<ContextImpl>> mContexts;

    Bitfield<static_cast<size_t>(ALC::EXTENSION_MAX)> mHasExt;

    std::once_flag mSetExts;
    void setupExts();

public:
    DeviceImpl(const char *name);
    ~DeviceImpl();

    ALCdevice *getALCdevice() const { return mDevice; }

    bool hasExtension(ALC ext) const { return mHasExt[static_cast<size_t>(ext)]; }

    LPALCDEVICEPAUSESOFT alcDevicePauseSOFT{nullptr};
    LPALCDEVICERESUMESOFT alcDeviceResumeSOFT{nullptr};

    LPALCGETSTRINGISOFT alcGetStringiSOFT{nullptr};
    LPALCRESETDEVICESOFT alcResetDeviceSOFT{nullptr};

    void removeContext(ContextImpl *ctx);

    String getName(PlaybackName type) const;
    bool queryExtension(const char *name) const;

    Version getALCVersion() const;
    Version getEFXVersion() const;

    ALCuint getFrequency() const;

    ALCuint getMaxAuxiliarySends() const;

    Vector<String> enumerateHRTFNames() const;
    bool isHRTFEnabled() const;
    String getCurrentHRTF() const;
    void reset(ArrayView<AttributePair> attributes);

    Context createContext(ArrayView<AttributePair> attributes);

    void pauseDSP();
    void resumeDSP();

    std::chrono::nanoseconds getClockTime();

    void close();
};

} // namespace alure

#endif /* DEVICE_H */
