#include <nextclient/plugin.hpp>
#include <tao/json.hpp>
#include <array>
#include <deque>
#include <string_view>
#include "tracker.h"

NC_MANIFEST(R"json({
  "schema":1,"id":"org.nextclient.life_stats","name":"Life Stats","author":"NextClient",
  "description":"Shows damage taken, kills, assists and your killer in local chat and console after death or round end.",
  "translations":{"ru":{"name":"Статистика жизни","description":"Показывает полученный урон, убийства, помощь в убийствах и вашего убийцу в локальном чате и консоли после смерти или окончания раунда."}},
  "version":"1.0.0","sdk":"1.0.0","abi":1,"api":1,"compatibility_revision":1,
  "permissions":["chat.print"]
})json")

class LifeStats final : public nextclient::Plugin
{
    life_stats::Tracker tracker_;
    std::array<std::string, 33> names_;
    std::deque<std::string> chat_;
    double clock_{}, next_chat_{}, game_time_{};
    bool discard_backlog_{}, await_boundary_{};

    void Reset()
    {
        tracker_.Reset();
        names_.fill({});
        chat_.clear();
        next_chat_ = clock_;
    }
    void Observe(bool eventBoundary = false, double eventTime = 0)
    {
        NcSession current{};
        if (session(current))
            game_time_ = current.time;
        NcPlayerState state{};
        if (player(state))
            tracker_.Observe(
                state.index,
                (eventBoundary || state.health > 0) && (state.flags & NC_PLAYER_ACTIVE),
                eventBoundary ? eventTime : game_time_,
                eventBoundary ? -1 : state.health
            );
    }
    static std::string Colored(const std::string& line)
    {
        constexpr std::string_view prefix = "[Life Stats]";
        return line.starts_with(prefix) ? "\x04[Life Stats]\x01" + line.substr(prefix.size()) : line;
    }
    std::string Name(int index)
    {
        if (index < 1 || index > 32)
            return {};
        NcPlayerInfo info{};
        if (player_info(index, info))
            names_[index] = info.name;
        return names_[index];
    }

public:
    void load() override
    {
        Reset();
        for (const auto* name :
             {"player.damage",
              "player.health",
              "player.death",
              "round.end",
              "round.start",
              "hud.reset",
              "hud.init",
              "connection.changed",
              "map.changed",
              "player.left",
              "player.joined"})
            if (!subscribe_event(name))
                throw std::exception();
    }
    void event(const char* raw, const char* json) override
    {
        const std::string_view name(raw);
        const auto data = tao::json::from_string(json);
        if (name == "sdk.overflow")
        {
            Reset();
            discard_backlog_ = await_boundary_ = true;
            const auto warning = Colored("[Life Stats] Events were lost; statistics resume at the next spawn or round.");
            console_print(warning.c_str());
            chat_.push_back(warning);
            return;
        }
        // An overflow notice precedes the surviving old queue. Even a reset in
        // that queue can predate the lost events, so it cannot establish a life.
        if (discard_backlog_)
            return;
        if (await_boundary_)
        {
            if (name != "hud.reset" && name != "hud.init" && name != "map.changed" && name != "round.start")
                return;
            Reset();
            await_boundary_ = false;
        }
        if (name == "hud.init" || name == "map.changed")
        {
            Reset();
            return;
        }
        if (name == "connection.changed")
        {
            if (!data.at("connected").get_boolean())
                Reset();
            return;
        }
        if (name == "player.left" || name == "player.joined")
        {
            const int index = data.at("index").as<int>();
            if (index >= 1 && index <= 32)
                names_[index] = name == "player.joined" ? data.at("name").get_string() : "";
            return;
        }
        const auto* timestamp = data.find("time");
        const double time = timestamp ? timestamp->as<double>() : game_time_;
        if (name == "hud.reset")
        {
            if (!tracker_.Local())
            {
                Observe();
            }
            tracker_.ResetHud();
            return;
        }
        if (!tracker_.Local())
            Observe(true, time);
        if (name == "player.damage")
            tracker_.Damage(data.at("health").as<int>(), data.at("armor").as<int>(), data.optional<uint32_t>("bits").value_or(0), time);
        else if (name == "player.health" && data.find("health"))
        {
            NcPlayerState state{};
            const bool active_player = player(state) && (state.flags & NC_PLAYER_ACTIVE);
            tracker_.Health(data.at("health").as<int>(), time, active_player);
        }
        else if (name == "player.death")
        {
            const int killer = data.at("killer").as<int>(), victim = data.at("victim").as<int>();
            const int other = victim == tracker_.Local() ? killer : victim;
            life_stats::DeathDetails extras;
            extras.assister = data.optional<int>("assister").value_or(-1);
            extras.assister_name = Name(extras.assister);
            if (const auto* details = data.find("kill_details"))
            {
                constexpr std::pair<const char*, const char*> tags[]{
                    {"killer_blind", "blinded killer"},
                    {"noscope", "no-scope"},
                    {"penetrated", "through wall"},
                    {"through_smoke", "through smoke"},
                    {"assisted_flash", "flash assist"},
                    {"domination_began", "domination began"},
                    {"domination", "domination"},
                    {"revenge", "revenge"},
                    {"in_air", "airborne killer"}
                };
                for (const auto& [key, text] : tags)
                    if (details->optional<bool>(key).value_or(false) &&
                        (std::string_view(key) != "domination" || !details->optional<bool>("domination_began").value_or(false)))
                        extras.tags.emplace_back(text);
            }
            tracker_.Death(killer, victim, Name(other), data.at("weapon").get_string(), data.at("headshot").get_boolean(), time, extras);
        }
        else if (name == "round.end")
            tracker_.RoundEnd(time);
        else if (name == "round.start")
            tracker_.NewRound();
    }
    void frame(const NcSession& state) override
    {
        clock_ += state.frame_time;
        game_time_ = state.time;
        if (!(state.flags & NC_SESSION_CONNECTED))
        {
            Reset();
            discard_backlog_ = await_boundary_ = false;
            return;
        }
        bool backlog = true;
        try
        {
            backlog = tao::json::from_string(extension("nextclient.events", "stats")).at("queued").as<size_t>() != 0;
        }
        catch (...)
        {} // Without queue state, postponing a report is safer than inventing one.
        if (!backlog)
        {
            discard_backlog_ = false;
            if (!await_boundary_)
            {
                Observe();
                tracker_.Tick(state.frame_time);
            }
        }
        const auto report = tracker_.TakeReport();
        for (const auto& line : report.console)
            console_print(Colored(line).c_str());
        for (const auto& line : report.chat)
            if (chat_.size() < 1024)
                chat_.push_back(Colored(line));
        if (!chat_.empty() && clock_ >= next_chat_)
        {
            if (chat_print(chat_.front().c_str()))
                chat_.pop_front();
            next_chat_ = clock_ + 0.6;
        }
        for (int i = 1; i <= state.max_clients && i <= 32; ++i)
            Name(i);
    }
};
NC_PLUGIN(LifeStats)
