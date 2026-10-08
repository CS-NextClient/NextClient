#pragma once

#include <array>
#include <span>

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>

class DeathMarks
{
public:
    struct Visible
    {
        ncl_math::Vector2 pos; // world xy
        ncl_math::Color color;
        int alpha; // 0..255
    };

    static constexpr int kMax = 32;
    static constexpr float kHold = 3.0f;
    static constexpr float kLifetime = 4.5f; // includes kHold

private:
    struct Mark
    {
        ncl_math::Vector2 pos;
        ncl_math::Color color;
        float time;
        bool active;
    };

    std::array<Mark, kMax> marks_{};
    int head_{};

public:
    void Reset();
    void Add(ncl_math::Vector2 world, ncl_math::Color color, float time);

    int CollectVisible(std::span<Visible> out, float time);
};
