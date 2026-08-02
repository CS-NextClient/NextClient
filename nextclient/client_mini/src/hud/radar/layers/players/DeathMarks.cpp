#include "DeathMarks.h"

#include <ncl_math/scalar.h>

void DeathMarks::Reset()
{
    for (Mark& mark : marks_)
    {
        mark.active = false;
    }
}

void DeathMarks::Add(ncl_math::Vector2 world, ncl_math::Color color, float time)
{
    Mark& mark = marks_[head_];
    mark.pos = world;
    mark.color = color;
    mark.time = time;
    mark.active = true;
    head_ = (head_ + 1) % kMax;
}

int DeathMarks::CollectVisible(std::span<Visible> out, float time)
{
    int result = 0;

    for (Mark& mark : marks_)
    {
        if (!mark.active || result >= (int)out.size())
        {
            continue;
        }

        float age = time - mark.time;

        if (age < 0.0f || age >= kLifetime)
        {
            mark.active = false;
            continue;
        }

        float vis = ncl_math::HoldLinear(age, kHold, kLifetime);
        out[result++] = {mark.pos, mark.color, (int)(vis * 255.0f)};
    }

    return result;
}
