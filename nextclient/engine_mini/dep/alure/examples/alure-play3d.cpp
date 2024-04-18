/*
 * A simple example showing how to load and play a sound.
 */

#include <string.h>

#include <cmath>
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "alure2.h"

#ifndef M_PI
#define M_PI                         (3.14159265358979323846)
#endif


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

        /* Make sure 3D spatialization is on (default for mono sources, not for
         * multi-channel) and set its initial position in front.
         */
        source.set3DSpatialize(alure::Spatialize::On);
        source.setPosition({0.0f, 0.0f, -1.0f});
        source.play(buffer);

        std::cout<< "Playing "<<args.front()<<" ("
                 << alure::GetSampleTypeName(buffer.getSampleType())<<", "
                 << alure::GetChannelConfigName(buffer.getChannelConfig())<<", "
                 << buffer.getFrequency()<<"hz)" <<std::endl;

        double angle{0.0};
        while(source.isPlaying())
        {
            std::cout<< "\r "<<source.getSampleOffset()<<" / "<<buffer.getLength() <<std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            /* Rotate the source around the origin by about 1/4 cycle per
             * second, and keep it within -pi...+pi.
             */
            angle += 0.01 * M_PI * 0.5;
            if(angle > M_PI)
                angle -= M_PI*2.0;
            source.setPosition({(float)sin(angle), 0.0f, -(float)cos(angle)});

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
