/*
 * A simple example showing how to stream a file through a source.
 */

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>

#include "alure2.h"


namespace {

// Helper class+method to print the time with human-readable formatting.
struct PrettyTime {
    alure::Seconds mTime;
};
inline std::ostream &operator<<(std::ostream &os, const PrettyTime &rhs)
{
    using hours = std::chrono::hours;
    using minutes = std::chrono::minutes;
    using seconds = std::chrono::seconds;
    using centiseconds = std::chrono::duration<int64_t, std::ratio<1, 100>>;
    using std::chrono::duration_cast;

    centiseconds t = duration_cast<centiseconds>(rhs.mTime);
    if(t.count() < 0)
    {
        os << '-';
        t *= -1;
    }

    // Only handle up to hour formatting
    if(t >= hours(1))
        os << duration_cast<hours>(t).count() << 'h' << std::setfill('0') << std::setw(2)
           << duration_cast<minutes>(t).count() << 'm';
    else
        os << duration_cast<minutes>(t).count() << 'm' << std::setfill('0');
    os << std::setw(2) << (duration_cast<seconds>(t).count() % 60) << '.' << std::setw(2)
       << (t.count() % 100) << 's' << std::setw(0) << std::setfill(' ');
    return os;
}

} // namespace

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
        alure::SharedPtr<alure::Decoder> decoder = ctx.createDecoder(args.front());
        alure::Source source = ctx.createSource();

        source.play(decoder, 12000, 4);
        std::cout<< "Playing "<<args.front()<<" ("
                 << alure::GetSampleTypeName(decoder->getSampleType())<<", "
                 << alure::GetChannelConfigName(decoder->getChannelConfig())<<", "
                 << decoder->getFrequency()<<"hz)" <<std::endl;

        double invfreq = 1.0 / decoder->getFrequency();
        while(source.isPlaying())
        {
            std::cout<< "\r "<<PrettyTime{source.getSecOffset()}<<" / "<<
                PrettyTime{alure::Seconds(decoder->getLength()*invfreq)};
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            ctx.update();
        }
        std::cout<<std::endl;

        source.destroy();
    }

    alure::Context::MakeCurrent(nullptr);
    ctx.destroy();
    dev.close();

    return 0;
}
