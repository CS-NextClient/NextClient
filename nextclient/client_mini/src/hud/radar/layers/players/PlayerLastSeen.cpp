#include "PlayerLastSeen.h"

#include <algorithm>

#include <ncl_math/scalar.h>

#include "hud/radar/layers/players/DeathMarks.h"

namespace
{
    constexpr float kReacquireRadius = 256.0f;
}

void PlayerLastSeen::Update(std::span<const Observation> visible, float time)
{
    std::array<Observation, kRadarMaxPlayers + 1> current{};
    for (const Observation& observation : visible)
    {
        current[observation.player] = observation;
    }

    for (int player = 1; player <= kRadarMaxPlayers; player++)
    {
        const Observation& previous = previous_[player];
        if (previous.player != 0 && current[player].player == 0)
        {
            marks_.push_back({player, previous.pos, previous.team, time, 255});
        }
    }

    // Clear after adding losses so a nearby enemy seen in this sweep also clears a new mark.
    std::erase_if(marks_, [visible, time](const Mark& mark) {
        float age = time - mark.time;
        if (age < 0.0f || age >= DeathMarks::kLifetime)
        {
            return true;
        }
        return std::ranges::any_of(visible, [&mark](const Observation& observation) {
            return (observation.pos - mark.pos).LengthSqr() <= kReacquireRadius * kReacquireRadius;
        });
    });
    for (Mark& mark : marks_)
    {
        float visibility = ncl_math::HoldLinear(time - mark.time, DeathMarks::kHold, DeathMarks::kLifetime);
        mark.alpha = (int)(visibility * 255.0f);
    }
    previous_ = current;
}

void PlayerLastSeen::Forget(int player)
{
    previous_[player] = Observation{};
}

void PlayerLastSeen::OnPlayerDeath(int player)
{
    Forget(player);
    std::erase_if(marks_, [player](const Mark& mark) { return mark.player == player; });
}

void PlayerLastSeen::Reset()
{
    previous_.fill(Observation{});
    marks_.clear();
}
