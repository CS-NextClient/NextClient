#pragma once

#include <cstdint>

#include "hud/radar/policy/RadarPolicy.h"
#include "hud/radar/RadarTeam.h"

// Which feed an objective marker is drawn from this frame. The choice depends only on who is watching,
// on what the policy made of the server's messages and on how old the last message is, so it is
// decided here, away from the entity and drawing code it steers.

enum class BombSource : std::uint8_t
{
    None,
    ServerMarker,
    Broadcast,
    Sight,
};

enum class HostageSource : std::uint8_t
{
    ServerMarker,
    Sight,
    Revealed,
};

struct ObjectiveAudience
{
    RadarTeam local_team;
    bool spectating;
};

BombSource ObjectiveSource_ChooseBomb(BombIntent intent, const ObjectiveAudience& audience);

// A dropped bomb is reported to living Terrorists alone, again every second, and so is its pickup:
// a drop report that stopped coming describes a bomb this viewer no longer hears about. A plant and
// its clears reach everyone, so a plant report holds until cleared. Spectators use the snapshot
// for drops because a retained report predates observing. age is in seconds.
bool ObjectiveSource_IsBombReportCurrent(bool planted, float age, bool spectating = false);

HostageSource ObjectiveSource_ChooseHostages(HostageIntent intent, const ObjectiveAudience& audience, bool hostages_revealed);
