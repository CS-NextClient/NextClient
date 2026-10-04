#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace life_stats
{
    std::string DisplayText(const std::string& value, size_t limit = 48);

    struct Report
    {
        std::vector<std::string> console, chat;
        bool empty() const
        {
            return console.empty() && chat.empty();
        }
    };

    struct DeathDetails
    {
        int assister{-1}; // Absent on stock servers; zero means explicitly none.
        std::string assister_name;
        std::vector<std::string> tags;
    };

    class Tracker
    {
    public:
        void Observe(int local, bool alive, double time, int health = -1);
        void Health(int health, double time);
        void Damage(int health, int armor, uint32_t bits, double time);
        void Death(
            int killer,
            int victim,
            const std::string& name,
            const std::string& weapon,
            bool headshot,
            double time,
            const DeathDetails& extras = {}
        );
        void RoundEnd(double time);
        void NewRound();
        void Spawn();
        void Reset();
        void Tick(double elapsed);
        Report TakeReport();
        int Local() const
        {
            return local_;
        }

    private:
        enum class End
        {
            None,
            Death,
            Round
        };
        struct Entry
        {
            double time{};
            bool damage{}, lethal{};
            int health{}, armor{}, overkill{};
            uint32_t bits{};
            std::string text;
        };
        int local_{}, remaining_health_{-1}, reported_health_{-1};
        bool alive_{}, active_{}, round_closed_{};
        uint64_t health_{}, armor_{}, overkill_{}, kills_{}, assists_{};
        End end_{End::None};
        double end_time_{}, pending_elapsed_{};
        std::string killer_, killer_console_, fatal_source_;
        std::vector<Entry> entries_;
        std::vector<std::string> victims_;
        Report reports_;
        void Begin();
        void Finish(End end, double time);
        void Flush();
    };
} // namespace life_stats
