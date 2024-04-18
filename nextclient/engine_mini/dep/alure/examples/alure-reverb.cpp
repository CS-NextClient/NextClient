/*
 * An example showing how to load and apply a reverb effect to a source.
 */

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>

#include "alure2.h"

#include "efx-presets.h"

namespace {

// Not UTF-8 aware!
int ci_compare(alure::StringView lhs, alure::StringView rhs)
{
    using traits = alure::StringView::traits_type;

    auto left = lhs.begin();
    auto right = rhs.begin();
    for(;left != lhs.end() && right != rhs.end();++left,++right)
    {
        int diff = std::tolower(traits::to_int_type(*left)) -
                   std::tolower(traits::to_int_type(*right));
        if(diff != 0) return (diff < 0) ? -1 : 1;
    }
    if(right != rhs.end()) return -1;
    if(left != lhs.end()) return 1;
    return 0;
}

#define DECL(x) { #x, EFX_REVERB_PRESET_##x }
const struct ReverbEntry {
    const char name[32];
    EFXEAXREVERBPROPERTIES props;
} reverblist[] = {
    DECL(GENERIC),
    DECL(PADDEDCELL),
    DECL(ROOM),
    DECL(BATHROOM),
    DECL(LIVINGROOM),
    DECL(STONEROOM),
    DECL(AUDITORIUM),
    DECL(CONCERTHALL),
    DECL(CAVE),
    DECL(ARENA),
    DECL(HANGAR),
    DECL(CARPETEDHALLWAY),
    DECL(HALLWAY),
    DECL(STONECORRIDOR),
    DECL(ALLEY),
    DECL(FOREST),
    DECL(CITY),
    DECL(MOUNTAINS),
    DECL(QUARRY),
    DECL(PLAIN),
    DECL(PARKINGLOT),
    DECL(SEWERPIPE),
    DECL(UNDERWATER),
    DECL(DRUGGED),
    DECL(DIZZY),
    DECL(PSYCHOTIC),

    DECL(CASTLE_SMALLROOM),
    DECL(CASTLE_SHORTPASSAGE),
    DECL(CASTLE_MEDIUMROOM),
    DECL(CASTLE_LARGEROOM),
    DECL(CASTLE_LONGPASSAGE),
    DECL(CASTLE_HALL),
    DECL(CASTLE_CUPBOARD),
    DECL(CASTLE_COURTYARD),
    DECL(CASTLE_ALCOVE),

    DECL(FACTORY_SMALLROOM),
    DECL(FACTORY_SHORTPASSAGE),
    DECL(FACTORY_MEDIUMROOM),
    DECL(FACTORY_LARGEROOM),
    DECL(FACTORY_LONGPASSAGE),
    DECL(FACTORY_HALL),
    DECL(FACTORY_CUPBOARD),
    DECL(FACTORY_COURTYARD),
    DECL(FACTORY_ALCOVE),

    DECL(ICEPALACE_SMALLROOM),
    DECL(ICEPALACE_SHORTPASSAGE),
    DECL(ICEPALACE_MEDIUMROOM),
    DECL(ICEPALACE_LARGEROOM),
    DECL(ICEPALACE_LONGPASSAGE),
    DECL(ICEPALACE_HALL),
    DECL(ICEPALACE_CUPBOARD),
    DECL(ICEPALACE_COURTYARD),
    DECL(ICEPALACE_ALCOVE),

    DECL(SPACESTATION_SMALLROOM),
    DECL(SPACESTATION_SHORTPASSAGE),
    DECL(SPACESTATION_MEDIUMROOM),
    DECL(SPACESTATION_LARGEROOM),
    DECL(SPACESTATION_LONGPASSAGE),
    DECL(SPACESTATION_HALL),
    DECL(SPACESTATION_CUPBOARD),
    DECL(SPACESTATION_ALCOVE),

    DECL(WOODEN_SMALLROOM),
    DECL(WOODEN_SHORTPASSAGE),
    DECL(WOODEN_MEDIUMROOM),
    DECL(WOODEN_LARGEROOM),
    DECL(WOODEN_LONGPASSAGE),
    DECL(WOODEN_HALL),
    DECL(WOODEN_CUPBOARD),
    DECL(WOODEN_COURTYARD),
    DECL(WOODEN_ALCOVE),

    DECL(SPORT_EMPTYSTADIUM),
    DECL(SPORT_SQUASHCOURT),
    DECL(SPORT_SMALLSWIMMINGPOOL),
    DECL(SPORT_LARGESWIMMINGPOOL),
    DECL(SPORT_GYMNASIUM),
    DECL(SPORT_FULLSTADIUM),
    DECL(SPORT_STADIUMTANNOY),

    DECL(PREFAB_WORKSHOP),
    DECL(PREFAB_SCHOOLROOM),
    DECL(PREFAB_PRACTISEROOM),
    DECL(PREFAB_OUTHOUSE),
    DECL(PREFAB_CARAVAN),

    DECL(DOME_TOMB),
    DECL(PIPE_SMALL),
    DECL(DOME_SAINTPAULS),
    DECL(PIPE_LONGTHIN),
    DECL(PIPE_LARGE),
    DECL(PIPE_RESONANT),

    DECL(OUTDOORS_BACKYARD),
    DECL(OUTDOORS_ROLLINGPLAINS),
    DECL(OUTDOORS_DEEPCANYON),
    DECL(OUTDOORS_CREEK),
    DECL(OUTDOORS_VALLEY),

    DECL(MOOD_HEAVEN),
    DECL(MOOD_HELL),
    DECL(MOOD_MEMORY),

    DECL(DRIVING_COMMENTATOR),
    DECL(DRIVING_PITGARAGE),
    DECL(DRIVING_INCAR_RACER),
    DECL(DRIVING_INCAR_SPORTS),
    DECL(DRIVING_INCAR_LUXURY),
    DECL(DRIVING_FULLGRANDSTAND),
    DECL(DRIVING_EMPTYGRANDSTAND),
    DECL(DRIVING_TUNNEL),

    DECL(CITY_STREETS),
    DECL(CITY_SUBWAY),
    DECL(CITY_MUSEUM),
    DECL(CITY_LIBRARY),
    DECL(CITY_UNDERPASS),
    DECL(CITY_ABANDONED),

    DECL(DUSTYROOM),
    DECL(CHAPEL),
    DECL(SMALLWATERROOM),
};
#undef DECL


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
        std::cerr<< "Usage: "<<args.front()<<" [-device \"device name\"] [-preset \"reverb preset\"] files..." <<std::endl;
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

    alure::Effect effect = ctx.createEffect();
    effect.setReverbProperties(EFX_REVERB_PRESET_GENERIC);

    if(args.size() > 1 && alure::StringView("-preset") == args[0])
    {
        alure::StringView reverb_name = args[1];
        args = args.slice(2);

        auto iter = std::find_if(std::begin(reverblist), std::end(reverblist),
            [reverb_name](const ReverbEntry &entry) -> bool
            { return ci_compare(reverb_name, entry.name) == 0; }
        );
        if(iter != std::end(reverblist))
        {
            std::cout<< "Loading preset "<<iter->name <<std::endl;
            effect.setReverbProperties(iter->props);
        }
        else
            std::cout<< "Failed to find preset "<<reverb_name <<std::endl;
    }
    else
        std::cout<< "Using generic reverb preset" <<std::endl;

    alure::AuxiliaryEffectSlot auxslot = ctx.createAuxiliaryEffectSlot();
    auxslot.applyEffect(effect);

    for(;!args.empty();args = args.slice(1))
    {
        alure::SharedPtr<alure::Decoder> decoder = ctx.createDecoder(args.front());
        alure::Source source = ctx.createSource();

        source.setAuxiliarySend(auxslot, 0);

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
        decoder.reset();
    }

    auxslot.destroy();
    effect.destroy();

    alure::Context::MakeCurrent(nullptr);
    ctx.destroy();
    dev.close();

    return 0;
}
