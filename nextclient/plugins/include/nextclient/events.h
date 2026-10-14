#pragma once
#include <nextclient/plugin.h>
#include <string_view>

namespace plugins
{
    // Shared by the host and client bridge. Chat must never enter a public event.
    inline uint32_t event_permission(std::string_view name)
    {
        if (name == "chat.message")
            return NC_PERMISSION_CHAT_READ;
        if (name == "cvar.changed")
            return NC_PERMISSION_CVARS_READ;
        constexpr std::string_view public_events[]{
            "player.health",   "player.armor",      "player.money",    "player.damage",      "player.death",     "player.team",
            "player.score",    "player.attributes", "player.location", "player.status",      "player.fov",       "player.weapon",
            "player.ammo",     "player.joined",     "player.left",     "weapon.definition",  "weapon.pickup",    "ammo.pickup",
            "item.pickup",     "round.time",        "round.reset",     "round.start",        "round.end",        "match.team_score",
            "match.mode",      "match.reset",       "bomb.dropped",    "bomb.picked_up",     "hostage.position", "hostage.killed",
            "hud.reset",       "hud.init",          "hud.status",      "hud.hide",           "hud.progress",     "hud.flashlight",
            "hud.nightvision", "hud.fade",          "hud.shake",       "connection.changed", "map.changed",      "voice.state"
        };
        for (auto event : public_events)
            if (event == name)
                return 0;
        return UINT32_MAX;
    }
} // namespace plugins
