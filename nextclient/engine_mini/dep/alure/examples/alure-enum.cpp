/*
 * An example showing how to enumerate available devices and show its
 * capabilities.
 */

#include <iostream>

#include "alure2.h"

int main(int argc, char *argv[])
{
    alure::DeviceManager devMgr = alure::DeviceManager::getInstance();
    alure::Vector<alure::String> list;
    alure::String defname;

    list = devMgr.enumerate(alure::DeviceEnumeration::Basic);
    defname = devMgr.defaultDeviceName(alure::DefaultDeviceType::Basic);
    std::cout<< "Available basic devices:\n";
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<'\n';
    std::cout<<std::endl;

    list = devMgr.enumerate(alure::DeviceEnumeration::Full);
    defname = devMgr.defaultDeviceName(alure::DefaultDeviceType::Full);
    std::cout<< "Available devices:\n";
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<'\n';
    std::cout<<std::endl;

    list = devMgr.enumerate(alure::DeviceEnumeration::Capture);
    defname = devMgr.defaultDeviceName(alure::DefaultDeviceType::Capture);
    std::cout<< "Available capture devices:\n";
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<'\n';
    std::cout<<std::endl;

    alure::Device dev = devMgr.openPlayback((argc > 1) ? argv[1] : "");
    std::cout<< "Info for device \""<<dev.getName(alure::PlaybackName::Full)<<"\":" <<std::endl;
    alure::Version version = dev.getALCVersion();
    std::cout<< "ALC version: "<<version.getMajor()<<"."<<version.getMinor() <<std::endl;
    version = dev.getEFXVersion();
    if(!version.isZero())
    {
        std::cout<< "EFX version: "<<version.getMajor()<<"."<<version.getMinor() <<'\n';
        std::cout<< "Max auxiliary sends: "<<dev.getMaxAuxiliarySends() <<std::endl;
    }
    else
        std::cout<< "EFX not supported" <<std::endl;
    dev.close();

    return 0;
}
