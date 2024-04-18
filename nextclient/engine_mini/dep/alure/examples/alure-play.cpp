/*
 * A simple example showing how to load and play a sound.
 */

#include <string.h>

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "alure2.h"

int main(int argc, char *argv[])
{
    alure::ArrayView<const char*> args(argv, argc);

    if(args.size() < 2)
    {
        std::cerr<< "Usage: "<<args.front()<<" [-device \"device name\"] files..." <<std::endl;
        return 1;
    }
    args = args.slice(1);

    alure::DeviceManager devMgr = alure::DeviceManager::getInstance();

    alure::Device dev;
    if(args.size() > 2 && args[0] == alure::StringView("-device"))
    {
        dev = devMgr.openPlayback(args[1], std::nothrow);
        if(!dev) std::cerr<< "Failed to open \""<<args[1]<<"\" - trying default" <<std::endl;
        args = args.slice(2);
    }
    if(!dev) dev = devMgr.openPlayback();
    std::cout<< "Opened \""<<dev.getName()<<"\"" <<std::endl;

    alure::Context ctx = dev.createContext();
    alure::Context::MakeCurrent(ctx);

    for(;!args.empty();args = args.slice(1))
    {
        alure::Buffer buffer = ctx.getBuffer(args.front());
        alure::Source source = ctx.createSource();
        source.play(buffer);
        std::cout<< "Playing "<<args.front()<<" ("
                 << alure::GetSampleTypeName(buffer.getSampleType())<<", "
                 << alure::GetChannelConfigName(buffer.getChannelConfig())<<", "
                 << buffer.getFrequency()<<"hz)" <<std::endl;

        while(source.isPlaying())
        {
            std::cout<< "\r "<<source.getSampleOffset()<<" / "<<buffer.getLength() <<std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            ctx.update();
        }
        std::cout<<std::endl;

        source.destroy();
        ctx.removeBuffer(buffer);
    }

    alure::Context::MakeCurrent(nullptr);
    ctx.destroy();
    dev.close();

    return 0;
}
