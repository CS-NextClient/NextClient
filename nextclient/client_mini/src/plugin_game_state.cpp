#include "plugin_game_state.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace
{
    using Json = tao::json::value;
    class Reader
    {
        const unsigned char* bytes_;
        size_t size_, cursor_{};

    public:
        Reader(const void* bytes, size_t size) :
            bytes_(static_cast<const unsigned char*>(bytes)),
            size_(size)
        {}
        int Byte()
        {
            if (cursor_ >= size_)
                throw std::runtime_error("Truncated game message");
            return bytes_[cursor_++];
        }
        int Short()
        {
            int lo = Byte(), hi = Byte();
            return static_cast<int16_t>(lo | (hi << 8));
        }
        int UnsignedShort()
        {
            return static_cast<uint16_t>(Short());
        }
        int32_t Long()
        {
            uint32_t value = 0;
            for (int i = 0; i < 4; ++i)
                value |= static_cast<uint32_t>(Byte()) << (8 * i);
            return static_cast<int32_t>(value);
        }
        std::string String()
        {
            std::string value;
            for (int c; (c = Byte()) != 0;)
                value += static_cast<char>(c);
            return value;
        }
        Json Position()
        {
            Json v = tao::json::empty_array;
            for (int i = 0; i < 3; ++i)
                v.push_back(Short() / 8.0f);
            return v;
        }
        bool Remaining() const
        {
            return cursor_ < size_;
        }
    };
    void PlayerIndex(int index)
    {
        if (index < 1 || index > 32)
            throw std::runtime_error("Invalid player index");
    }
    void OptionalPlayerIndex(int index)
    {
        if (index != 0)
            PlayerIndex(index);
    }
    void DeathExtras(Reader& r, Json& data)
    {
        // Stock DeathMsg ends after the weapon. ReGameDLL appends a flags word
        // followed by only the selected fields, in this order.
        if (!r.Remaining())
            return;
        const auto flags = static_cast<uint32_t>(r.Long());
        data["death_flags"] = flags;
        if (flags & 0x001)
            data["death_position"] = r.Position();
        if (flags & 0x002)
        {
            const int assister = r.Byte();
            OptionalPlayerIndex(assister);
            data["assister"] = assister;
        }
        if (flags & 0x004)
        {
            const auto rarity = static_cast<uint32_t>(r.Long());
            data["kill_flags"] = rarity;
            Json details = tao::json::empty_object;
            constexpr const char* names[]{
                "headshot",
                "killer_blind",
                "noscope",
                "penetrated",
                "through_smoke",
                "assisted_flash",
                "domination_began",
                "domination",
                "revenge",
                "in_air"
            };
            for (size_t i = 0; i < std::size(names); ++i)
                details[names[i]] = (rarity & (1u << i)) != 0;
            data["kill_details"] = std::move(details);
        }
    }
    void Merge(Json& target, const Json& fields)
    {
        if (!target.is_object())
            target = tao::json::empty_object;
        for (const auto& [key, value] : fields.get_object())
            target[key] = value;
    }
} // namespace

bool PluginGameState::Observes(std::string_view name)
{
    constexpr std::string_view names[]{"Health",     "HealthNEx",   "Battery",     "ArmorType",  "Money",      "Damage",      "DeathMsg",
                                       "ScoreInfo",  "ScoreAttrib", "TeamInfo",    "TeamScore",  "Location",   "Radar",       "SpecHealth2",
                                       "WeaponList", "CurWeapon",   "AmmoX",       "WeapPickup", "AmmoPickup", "ItemPickup",  "RoundTime",
                                       "HLTV",       "ResetHUD",    "InitHUD",     "GameMode",   "BombDrop",   "BombPickup",  "HostagePos",
                                       "HostageK",   "StatusIcon",  "StatusValue", "StatusText", "HideWeapon", "SetFOV",      "BarTime",
                                       "BarTime2",   "Flashlight",  "FlashBat",    "NVGToggle",  "ScreenFade", "ScreenShake", "SayText",
                                       "TextMsg",    "HealthInfo",  "Account"};
    for (auto value : names)
        if (value == name)
            return true;
    return false;
}
void PluginGameState::Reset()
{
    round_active_ = false;
    players_.clear();
    weapons_.clear();
    ammo_.clear();
    match_ = tao::json::empty_object;
}
void PluginGameState::RemovePlayer(int index)
{
    players_.erase(index);
}
tao::json::value PluginGameState::Player(int index) const
{
    auto it = players_.find(index);
    return it == players_.end() ? Json(tao::json::empty_object) : it->second;
}
tao::json::value PluginGameState::Weapon(int index) const
{
    auto it = weapons_.find(index);
    return it == weapons_.end() ? Json(tao::json::null) : it->second;
}
tao::json::value PluginGameState::Ammo() const
{
    Json result = tao::json::empty_array;
    for (int i = 0; i < 32; ++i)
    {
        auto it = ammo_.find(i);
        result.push_back(it == ammo_.end() ? Json(tao::json::null) : it->second);
    }
    return result;
}
tao::json::value PluginGameState::Match() const
{
    return match_;
}

int PluginGameState::Armor() const
{
    const auto* state = match_.find("player.armor");
    const auto* armor = state ? state->find("armor") : nullptr;
    return armor ? armor->as<int>() : 0;
}

std::vector<PluginGameEvent> PluginGameState::Message(std::string_view name, const void* bytes, size_t size, float time)
{
    std::vector<PluginGameEvent> result;
    if (!Observes(name) || size > 4096 || (!bytes && size) || !std::isfinite(time))
        return result;
    try
    {
        Reader r(bytes, size);
        Json data = tao::json::empty_object;
        std::string event;
        int player = 0, weapon = -1, ammo = -1;
        bool save_match = true;
        if (name == "Health" || name == "HealthNEx")
        {
            data["health"] = name == "Health" ? r.Byte() : r.Long();
            event = "player.health";
        }
        else if (name == "HealthInfo" || name == "Account")
        {
            player = r.Byte();
            PlayerIndex(player);
            const int value = r.Long();
            const bool health = name == "HealthInfo";
            data[health ? "reported_health" : "reported_money"] = value == -1 ? Json(tao::json::null) : Json(value);
            event = health ? "player.health" : "player.money";
        }
        else if (name == "Battery")
        {
            data["armor"] = r.Short();
            event = "player.armor";
        }
        else if (name == "ArmorType")
        {
            data["armor_type"] = r.Byte();
            event = "player.armor";
        }
        else if (name == "Money")
        {
            data["money"] = r.Long();
            data["flash"] = r.Byte() != 0;
            event = "player.money";
        }
        else if (name == "Damage")
        {
            data["armor"] = r.Byte();
            data["health"] = r.Byte();
            data["bits"] = static_cast<uint32_t>(r.Long());
            data["position"] = r.Position();
            event = "player.damage";
            save_match = false;
        }
        else if (name == "DeathMsg")
        {
            const int killer = r.Byte();
            OptionalPlayerIndex(killer);
            data["killer"] = killer;
            player = r.Byte();
            PlayerIndex(player);
            data["victim"] = player;
            data["headshot"] = r.Byte() != 0;
            data["weapon"] = r.String();
            DeathExtras(r, data);
            event = "player.death";
            save_match = false;
        }
        else if (name == "ScoreInfo")
        {
            player = r.Byte();
            PlayerIndex(player);
            data["frags"] = r.Short();
            data["deaths"] = r.Short();
            data["class"] = r.Short();
            data["team_id"] = r.Short();
            event = "player.score";
        }
        else if (name == "ScoreAttrib")
        {
            player = r.Byte();
            PlayerIndex(player);
            const int flags = r.Byte();
            data["attribute_flags"] = flags;
            data["dead"] = (flags & 1) != 0;
            data["has_c4"] = (flags & 2) != 0;
            data["vip"] = (flags & 4) != 0;
            // An unset bit can mean unsupported/hidden, not necessarily no kit.
            data["has_defuser"] = (flags & 8) ? Json(true) : Json(tao::json::null);
            event = "player.attributes";
        }
        else if (name == "TeamInfo")
        {
            player = r.Byte();
            PlayerIndex(player);
            data["team"] = r.String();
            event = "player.team";
        }
        else if (name == "TeamScore")
        {
            data["team"] = r.String();
            data["score"] = r.Short();
            event = "match.team_score";
        }
        else if (name == "Location")
        {
            player = r.Byte();
            PlayerIndex(player);
            data["location"] = r.String();
            event = "player.location";
        }
        else if (name == "Radar")
        {
            player = r.Byte();
            PlayerIndex(player);
            data["radar_position"] = r.Position();
            event = "player.location";
        }
        else if (name == "SpecHealth2")
        {
            data["reported_health"] = r.Byte();
            player = r.Byte();
            PlayerIndex(player);
            event = "player.health";
        }
        else if (name == "WeaponList")
        {
            data["name"] = r.String();
            const int a = r.Byte();
            data["ammo_type"] = a == 255 ? -1 : a;
            data["ammo_max"] = r.Byte();
            const int b = r.Byte();
            data["ammo2_type"] = b == 255 ? -1 : b;
            data["ammo2_max"] = r.Byte();
            data["slot"] = r.Byte();
            data["slot_position"] = r.Byte();
            weapon = r.Byte();
            if (weapon < 1 || weapon >= 64)
                return {};
            data["id"] = weapon;
            data["flags"] = r.Byte();
            event = "weapon.definition";
        }
        else if (name == "CurWeapon")
        {
            data["state"] = r.Byte();
            data["id"] = r.Byte();
            int clip = r.Byte();
            data["clip"] = clip == 255 ? -1 : clip;
            event = "player.weapon";
        }
        else if (name == "AmmoX" || name == "AmmoPickup")
        {
            const int id = r.Byte();
            if (id > 31)
                return {};
            data["id"] = id;
            data["amount"] = r.Byte();
            if (name == "AmmoX")
            {
                ammo = id;
                event = "player.ammo";
            }
            else
            {
                event = "ammo.pickup";
                save_match = false;
            }
        }
        else if (name == "WeapPickup")
        {
            data["id"] = r.Byte();
            event = "weapon.pickup";
            save_match = false;
        }
        else if (name == "ItemPickup")
        {
            data["name"] = r.String();
            event = "item.pickup";
            save_match = false;
        }
        else if (name == "RoundTime")
        {
            data["seconds"] = r.Short();
            data["received_at"] = time;
            event = "round.time";
        }
        else if (name == "HLTV")
        {
            const int a = r.Byte(), b = r.Byte();
            if (a != 0 || b != 0)
                return {};
            event = "round.start";
        }
        else if (name == "ResetHUD")
        {
            event = "hud.reset";
        }
        else if (name == "InitHUD")
        {
            event = "hud.init";
        }
        else if (name == "GameMode")
        {
            data["mode"] = r.Byte();
            event = "match.mode";
        }
        else if (name == "BombDrop")
        {
            data["position"] = r.Position();
            data["planted"] = r.Byte() != 0;
            event = "bomb.dropped";
        }
        else if (name == "BombPickup")
        {
            event = "bomb.picked_up";
        }
        else if (name == "HostagePos")
        {
            data["active"] = r.Byte();
            data["id"] = r.Byte();
            data["position"] = r.Position();
            event = "hostage.position";
        }
        else if (name == "HostageK")
        {
            data["id"] = r.Byte();
            event = "hostage.killed";
        }
        else if (name == "StatusIcon")
        {
            data["state"] = r.Byte();
            data["icon"] = r.String();
            if (data["state"] != 0)
            {
                data["r"] = r.Byte();
                data["g"] = r.Byte();
                data["b"] = r.Byte();
            }
            event = "hud.status";
        }
        else if (name == "StatusValue")
        {
            data["id"] = r.Byte();
            data["value"] = r.Short();
            event = "player.status";
        }
        else if (name == "StatusText")
        {
            data["id"] = r.Byte();
            data["text"] = r.String();
            event = "player.status";
        }
        else if (name == "HideWeapon")
        {
            data["flags"] = r.Byte();
            event = "hud.hide";
        }
        else if (name == "SetFOV")
        {
            data["fov"] = r.Byte();
            event = "player.fov";
        }
        else if (name == "BarTime" || name == "BarTime2")
        {
            data["seconds"] = r.Short();
            data["percent"] = name == "BarTime2" ? r.Short() : 0;
            data["received_at"] = time;
            event = "hud.progress";
        }
        else if (name == "Flashlight")
        {
            data["enabled"] = r.Byte() != 0;
            data["battery"] = r.Byte();
            event = "hud.flashlight";
        }
        else if (name == "FlashBat")
        {
            data["battery"] = r.Byte();
            event = "hud.flashlight";
        }
        else if (name == "NVGToggle")
        {
            data["enabled"] = r.Byte() != 0;
            event = "hud.nightvision";
        }
        else if (name == "ScreenFade")
        {
            data["duration"] = r.UnsignedShort() / 4096.0f;
            data["hold"] = r.UnsignedShort() / 4096.0f;
            data["flags"] = r.UnsignedShort();
            data["r"] = r.Byte();
            data["g"] = r.Byte();
            data["b"] = r.Byte();
            data["a"] = r.Byte();
            event = "hud.fade";
        }
        else if (name == "ScreenShake")
        {
            data["amplitude"] = r.UnsignedShort() / 4096.0f;
            data["duration"] = r.UnsignedShort() / 4096.0f;
            data["frequency"] = r.UnsignedShort() / 256.0f;
            event = "hud.shake";
        }
        else if (name == "SayText" || name == "TextMsg")
        {
            const int first = r.Byte();
            const auto format = r.String();
            Json args = tao::json::empty_array;
            while (r.Remaining())
            {
                if (args.get_array().size() >= 4)
                    return {};
                args.push_back(r.String());
            }
            // Only fixed, recognized system tokens become permission-free round events.
            if (name == "TextMsg" &&
                (format == "#Terrorists_Win" || format == "#CTs_Win" || format == "#Round_Draw" || format == "#Target_Bombed" ||
                 format == "#Bomb_Defused" || format == "#Target_Saved" || format == "#All_Hostages_Rescued" ||
                 format == "#Hostages_Not_Rescued" || format == "#VIP_Escaped" || format == "#VIP_Assassinated" ||
                 format == "#VIP_Not_Escaped" || format == "#Terrorists_Escaped" || format == "#CTs_PreventEscape" ||
                 format == "#Escaping_Terrorists_Neutralized" || format == "#Terrorists_Not_Escaped"))
            {
                data["reason"] = format;
                event = "round.end";
            }
            else if (name == "TextMsg" && (format == "#Game_Commencing" || format == "#Game_will_restart_in"))
            {
                event = "match.reset";
                data["reason"] = format;
            }
            else
            {
                if (name == "TextMsg" && first != 3)
                    return {}; // HUD_PRINTTALK only.
                data["sender"] = name == "SayText" ? first : 0;
                data["format"] = format;
                data["arguments"] = args;
                data["text"] = format.starts_with("#Cstrike_Chat_") && !args.get_array().empty() ? args.get_array().back() : Json(format);
                data["team"] =
                    format.starts_with("#Cstrike_Chat_") ? Json(!format.starts_with("#Cstrike_Chat_All")) : Json(tao::json::null);
                event = "chat.message";
                save_match = false;
            }
        }
        if (event.empty())
            return {};
        // Validate UTF-8 before mutating persistent state; malformed messages are ignored.
        (void)tao::json::from_string(tao::json::to_string(data));
        if (name == "InitHUD")
        {
            // WeaponList is part of signon and can precede InitHUD. Map epochs
            // clear definitions; HUD initialization must retain them.
            players_.clear();
            ammo_.clear();
            match_ = tao::json::empty_object;
        }
        // ResetHUD reaches players at respawn; RoundTime also reaches spectators.
        // Refreshes during an active round must not erase its state.
        const bool round_start = name == "HLTV" || ((name == "ResetHUD" || name == "RoundTime") && !round_active_);
        if (round_start)
        {
            round_active_ = true;
            for (const auto* key : {"bomb.dropped", "bomb.picked_up", "round.end", "round.time", "hud.progress", "hostages"})
                match_.get_object().erase(key);
            for (auto& [id, fields] : players_)
            {
                fields.get_object().erase("reported_health");
                fields.get_object().erase("reported_money");
                fields.get_object().erase("has_defuser");
                fields.get_object().erase("attribute_flags");
                fields.get_object().erase("radar_position");
            }
        }
        if (name == "BombPickup")
            match_.get_object().erase("bomb.dropped");
        if (name == "BombDrop")
            match_.get_object().erase("bomb.picked_up");
        if (name == "ResetHUD")
            for (const auto* key : {"hud.progress", "hud.fade", "hud.shake", "status_icons", "StatusValue", "StatusText"})
                match_.get_object().erase(key);
        if (player)
        {
            if (name == "DeathMsg")
            {
                Merge(players_[player], Json{{"dead", true}});
            }
            else
                Merge(players_[player], data);
            data["player"] = player;
        }
        else if (weapon >= 0)
            weapons_[weapon] = data;
        else if (ammo >= 0)
            ammo_[ammo] = data.at("amount");
        else if (save_match)
        {
            for (const auto* key : {"team_scores", "status_icons", "StatusValue", "StatusText", "hostages"})
                if (!match_.find(key))
                    match_[key] = tao::json::empty_object;
            if (name == "TeamScore")
            {
                auto& scores = match_["team_scores"];
                const auto& team = data.at("team").get_string();
                // Mod-defined names are supported, but a server cannot grow the
                // snapshot indefinitely. Existing entries remain updateable.
                if (team.size() <= 64 && (scores.find(team) || scores.get_object().size() < 32))
                    scores[team] = data.at("score");
            }
            else if (name == "StatusIcon")
            {
                auto& icons = match_["status_icons"];
                const auto& icon = data.at("icon").get_string();
                if (data.at("state") == 0)
                    icons.get_object().erase(icon);
                else if (icon.size() <= 64 && (icons.find(icon) || icons.get_object().size() < 64))
                    icons[icon] = data;
            }
            else if (name == "StatusValue" || name == "StatusText")
                match_[std::string(name)][std::to_string(data.at("id").as<int>())] = data;
            else if (name == "HostagePos" || name == "HostageK")
                match_["hostages"][std::to_string(data.at("id").as<int>())] = data;
            else if (name == "Flashlight" || name == "FlashBat" || name == "Battery" || name == "ArmorType")
                Merge(match_[event], data);
            else
                match_[event] = data;
        }
        data["time"] = time;
        if (event == "round.end" || event == "match.reset")
        {
            round_active_ = false;
        }
        else if (name == "RoundTime")
        {
            round_active_ = true;
        }
        result.push_back({event, std::move(data)});
        if (round_start && name != "HLTV")
        {
            result.push_back({"round.start", Json{{"time", time}}});
        }
        if (name == "ResetHUD")
            result.push_back({"round.reset", Json{{"time", time}}});
    }
    catch (...)
    {
        return {};
    }
    return result;
}
