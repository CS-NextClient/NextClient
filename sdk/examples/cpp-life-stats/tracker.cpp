#include "tracker.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace life_stats
{
    std::string DisplayText(const std::string& value, size_t limit)
    {
        std::string result;
        for (unsigned char c : value)
            result += c < 32 || c == 127 || c == '%' ? ' ' : static_cast<char>(c);
        if (result.size() > limit)
        {
            size_t cut = limit;
            while (cut && (static_cast<unsigned char>(result[cut]) & 0xc0) == 0x80)
                --cut;
            result.resize(cut);
        }
        return result;
    }
    namespace
    {
        std::string Offset(double seconds)
        {
            const auto ms = static_cast<uint64_t>(std::llround(std::clamp(seconds, 0.0, 86400.0) * 1000));
            return std::format("(-{:02}:{:02}.{:03})", ms / 60000, ms / 1000 % 60, ms % 1000);
        }
        std::string DamageType(uint32_t bits)
        {
            // Damage flags describe the effect, not its attacker or weapon.
            if (bits & (1u << 5))
                return "Fall damage";
            if (bits & ((1u << 6) | (1u << 24)))
                return "Explosion damage";
            if (bits & (1u << 14))
                return "Drowning damage";
            if (bits & (1u << 3))
                return "Burn damage";
            if (bits & (1u << 1))
                return "Bullet damage";
            return "Damage taken";
        }
    } // namespace
    void Tracker::Begin()
    {
        active_ = true;
        hud_reset_ = false;
        end_ = End::None;
        health_ = armor_ = overkill_ = kills_ = assists_ = 0;
        remaining_health_ = reported_health_ = -1;
        killer_.clear();
        killer_console_.clear();
        fatal_source_.clear();
        entries_.clear();
        victims_.clear();
    }
    void Tracker::Observe(int local, bool alive, double time, int health)
    {
        if (local < 1 || local > 32)
            return;
        if (local_ != local)
        {
            Reset();
            local_ = local;
        }
        if (alive && !alive_ && !round_closed_)
        {
            if (end_ != End::None)
                Flush();
            if (!active_)
                Begin();
        }
        if (health >= 0 && (alive || health == 0))
        {
            UpdateHealth(health, time);
        }
        if (!alive && alive_)
            Finish(End::Death, time);
        alive_ = alive;
    }
    void Tracker::Health(int health, double time, bool active_player)
    {
        if (health < 0)
        {
            return;
        }
        if (hud_reset_)
        {
            hud_reset_ = false;
            if (health > 0 && active_player && local_ && !round_closed_ && (!active_ || end_ == End::Death))
            {
                Flush();
                Begin();
                alive_ = true;
            }
        }
        UpdateHealth(health, time);
    }
    void Tracker::UpdateHealth(int health, double time)
    {
        if (!active_)
        {
            return;
        }
        // Health commonly arrives before its Damage message. Retain the previous
        // HP until Damage consumes it; increases represent healing, not damage.
        if (remaining_health_ < 0 && health > 0)
            remaining_health_ = health;
        else if (reported_health_ >= 0 && health > reported_health_ && end_ == End::None)
            remaining_health_ += health - reported_health_;
        reported_health_ = health;
        if (!health)
            Finish(End::Death, time);
    }
    void Tracker::Damage(int health, int armor, uint32_t bits, double time)
    {
        // Damage messages can follow DeathMsg in the next network update.
        if (!active_ || health < 0 || health > 255 || armor < 0 || armor > 255 || !std::isfinite(time))
            return;
        const int applied = remaining_health_ >= 0 ? std::min(health, remaining_health_) : health;
        const int overkill = health - applied;
        if (remaining_health_ >= 0)
            remaining_health_ -= applied;
        health_ += applied;
        armor_ += armor;
        overkill_ += overkill;
        if (entries_.size() < 4096 && (health || armor))
            entries_.push_back({time, true, health > 0 && remaining_health_ == 0, applied, armor, overkill, bits, {}});
    }
    void Tracker::Finish(End end, double time)
    {
        if (!active_ || (end_ != End::None && !(end_ == End::Round && end == End::Death)))
            return;
        end_ = end;
        end_time_ = time;
        pending_elapsed_ = 0;
    }
    void Tracker::Death(
        int killer,
        int victim,
        const std::string& name,
        const std::string& weapon,
        bool headshot,
        double time,
        const DeathDetails& extras
    )
    {
        if (!active_ || victim < 1 || victim > 32 || killer < 0 || killer > 32)
            return;
        const auto display = DisplayText(name);
        auto details = DisplayText(weapon, 32);
        if (headshot)
            details += details.empty() ? "headshot" : ", headshot";
        if (!details.empty())
            details = " (" + details + ")";
        std::string extra_text;
        for (const auto& tag : extras.tags)
            extra_text += (extra_text.empty() ? "" : ", ") + DisplayText(tag);
        const auto assister = DisplayText(extras.assister_name);
        if (extras.assister > 0 && extras.assister <= 32 && extras.assister != local_ && !assister.empty())
            extra_text += (extra_text.empty() ? "" : ", ") + std::string("assisted by ") + assister;
        const auto console_details = details + (extra_text.empty() ? "" : " [" + extra_text + "]");
        if (victim == local_)
        {
            Finish(End::Death, time);
            end_time_ = time;
            if (killer == local_)
            {
                killer_ = "Self-inflicted death" + details + ".";
                killer_console_ = "Self-inflicted death" + console_details + ".";
                fatal_source_ = "Self-inflicted damage" + console_details;
            }
            else if (killer != 0 && !display.empty())
            {
                killer_ = "Killed by " + display + details + ".";
                killer_console_ = "Killed by " + display + console_details + ".";
                fatal_source_ = "Damage from " + display + console_details;
            }
        }
        else if (killer == local_ && end_ == End::None)
        {
            ++kills_;
            if (!display.empty() && victims_.size() < 512)
            {
                const auto text = "Killed " + display + details;
                victims_.push_back(text + ".");
                if (entries_.size() < 4096)
                    entries_.push_back({time, false, false, 0, 0, 0, 0, "Killed " + display + console_details});
            }
        }
        else if (extras.assister == local_ && killer != 0 && killer != victim && end_ == End::None)
        {
            ++assists_;
            if (!display.empty() && victims_.size() < 512)
            {
                victims_.push_back("Assisted in killing " + display + details + ".");
                if (entries_.size() < 4096)
                    entries_.push_back({time, false, false, 0, 0, 0, 0, "Assisted in killing " + display + console_details});
            }
        }
    }
    void Tracker::RoundEnd(double time)
    {
        Finish(End::Round, time);
        round_closed_ = true;
    }
    void Tracker::Flush()
    {
        if (!active_ || end_ == End::None)
            return;
        const std::string prefix = "[Life Stats] ";
        const std::string reason = end_ == End::Round ? "Round ended. " : "Life ended. ";
        auto summary = prefix + reason + "Damage taken: " + std::to_string(health_) + " HP, " + std::to_string(armor_) + " armor.";
        if (overkill_)
            summary += " Overkill: " + std::to_string(overkill_) + " HP.";
        summary += " Kills: " + std::to_string(kills_) + ".";
        if (assists_)
            summary += " Assists: " + std::to_string(assists_) + ".";
        reports_.chat.push_back(summary);
        reports_.console.push_back(summary);
        if (!killer_.empty())
            reports_.chat.push_back(prefix + killer_);
        for (const auto& victim : victims_)
            reports_.chat.push_back(prefix + victim);

        size_t fatal = entries_.size();
        if (end_ == End::Death)
            for (size_t i = entries_.size(); i > 0; --i)
                if (entries_[i - 1].lethal && std::abs(entries_[i - 1].time - end_time_) <= 0.5)
                {
                    fatal = i - 1;
                    break;
                }
        for (size_t i = 0; i < entries_.size(); ++i)
        {
            const auto& entry = entries_[i];
            std::string line = prefix;
            if (entry.damage)
            {
                line += i == fatal && !fatal_source_.empty() ? fatal_source_ : DamageType(entry.bits);
                line += " -" + std::to_string(entry.health) + " HP";
                if (entry.armor)
                    line += ", -" + std::to_string(entry.armor) + " armor";
                if (entry.overkill)
                    line += " (" + std::to_string(entry.overkill) + " HP overkill)";
            }
            else
                line += entry.text;
            line += i == fatal ? " (Death)" : " " + Offset(end_time_ - entry.time);
            reports_.console.push_back(std::move(line));
        }
        if (fatal == entries_.size() && !killer_console_.empty())
            reports_.console.push_back(prefix + killer_console_);
        active_ = false;
    }
    void Tracker::Tick(double elapsed)
    {
        if (end_ != End::None && std::isfinite(elapsed) && elapsed > 0)
        {
            pending_elapsed_ += elapsed;
            if (pending_elapsed_ >= 0.25)
                Flush();
        }
    }
    void Tracker::NewRound()
    {
        // Do not invent a delayed round-end report at the next round's start.
        Flush();
        active_ = alive_ = round_closed_ = false;
        hud_reset_ = true;
        end_ = End::None;
    }
    void Tracker::ResetHud()
    {
        // Full HUD updates also occur while alive or dead; only a new health event confirms respawn.
        hud_reset_ = true;
    }
    void Tracker::Reset()
    {
        *this = Tracker{};
    }
    Report Tracker::TakeReport()
    {
        return std::exchange(reports_, {});
    }
} // namespace life_stats
