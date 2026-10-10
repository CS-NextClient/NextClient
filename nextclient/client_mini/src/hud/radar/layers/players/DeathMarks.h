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
        ncl_math::Vector3 pos;
        ncl_math::Color color;
        int alpha; // 0..255
    };

private:
    struct Mark
    {
        ncl_math::Vector3 pos;
        ncl_math::Color color;
        float time;
        bool active;
    };

public:
    // Must precede marks_, whose type uses this array bound.
    static constexpr int kMax = 32;

private:
    std::array<Mark, kMax> marks_{};
    int head_{};

public:
    static constexpr float kHold = 3.0f;
    static constexpr float kLifetime = 4.5f; // includes kHold

    void Reset();
    void Add(ncl_math::Vector3 world, ncl_math::Color color, float time);

    int CollectVisible(std::span<Visible> out, float time);
};
