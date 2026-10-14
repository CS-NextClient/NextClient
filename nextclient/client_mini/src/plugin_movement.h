#pragma once
#include <nextclient/plugin.h>
#include <goldsrc/entity.h>
#include <goldsrc/pmove.h>
#include <goldsrc/usercmd.h>

// ACTIVE is supplied by the caller: command input and player snapshots have
// different activation rules. Prediction determines the shared movement flags.
inline uint32_t PluginMovementFlags(const goldsrc::playermove_t* movement)
{
    if (!movement)
        return 0;
    uint32_t flags = NC_PLAYER_VALID;
    if (movement->onground != -1)
        flags |= NC_PLAYER_GROUNDED;
    if (movement->oldbuttons & goldsrc::IN_JUMP)
        flags |= NC_PLAYER_JUMP_HELD;
    if (!movement->dead && !movement->deadflag && !movement->spectator && !movement->iuser1 && movement->movetype == goldsrc::MOVETYPE_WALK &&
        movement->waterlevel < 2 && movement->waterjumptime <= 0 && !(movement->flags & (goldsrc::FL_WATERJUMP | goldsrc::FL_FROZEN)))
        flags |= NC_PLAYER_CAN_JUMP;
    return flags;
}
