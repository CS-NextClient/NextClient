#include "ObjectiveSource.h"

namespace
{
    // CWeaponBox::BombThink repeats the drop report every second; three missed repeats end it.
    constexpr float kDropReportLifetime = 3.0f; // seconds
} // namespace

BombSource ObjectiveSource_ChooseBomb(BombIntent intent, const ObjectiveAudience& audience)
{
    if (intent == BombIntent::Marker)
    {
        return BombSource::ServerMarker;
    }

    // Spectators take the Terrorists' feed: the line-of-sight path cannot serve them, since
    // RadarVisibility keeps no viewers while spectating.
    if (audience.local_team == RadarTeam::Terrorist || audience.spectating)
    {
        return BombSource::Broadcast;
    }

    // The one bomb message a Counter-Terrorist receives is the plant, which is not theirs to see.
    if (audience.local_team == RadarTeam::CounterTerrorist)
    {
        return BombSource::Sight;
    }

    return BombSource::None;
}

bool ObjectiveSource_IsBombReportCurrent(bool planted, float age, bool spectating)
{
    return planted || (!spectating && age >= 0.0f && age <= kDropReportLifetime);
}

HostageSource ObjectiveSource_ChooseHostages(HostageIntent intent, const ObjectiveAudience& audience, bool hostages_revealed)
{
    if (intent == HostageIntent::Marker)
    {
        return HostageSource::ServerMarker;
    }

    // Terrorists are never sent hostage positions, nor is a CT whose server withholds the broadcast.
    bool los_gated = !audience.spectating && (audience.local_team == RadarTeam::Terrorist || !hostages_revealed);

    return los_gated ? HostageSource::Sight : HostageSource::Revealed;
}
