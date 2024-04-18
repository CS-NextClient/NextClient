#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class DeviceManagerImpl {
    static WeakPtr<DeviceManagerImpl> sInstance;

    Vector<UniquePtr<DeviceImpl>> mDevices;

public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    static SharedPtr<DeviceManagerImpl> getInstance();

    DeviceManagerImpl();
    ~DeviceManagerImpl();

    void removeDevice(DeviceImpl *dev);

    bool queryExtension(const char *name) const;

    Vector<String> enumerate(DeviceEnumeration type) const;
    String defaultDeviceName(DefaultDeviceType type) const;

    Device openPlayback(const char *name);
    Device openPlayback(const char *name, const std::nothrow_t&) noexcept;
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
