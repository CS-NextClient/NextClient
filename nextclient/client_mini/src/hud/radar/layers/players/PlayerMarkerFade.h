#pragma once

#include <cstdint>

#include <ncl_math/vec3.h>

class PlayerMarkerFade
{
public:
    struct Visible
    {
        ncl_math::Vector3 pos{};
        float alpha = 1.0f;
        ncl_math::Vector3 previous_pos{};
        float previous_alpha{};
    };

private:
    ncl_math::Vector3 pos_{};
    ncl_math::Vector3 previous_pos_{};
    std::uint32_t seq_{};
    float last_time_{};
    float start_{};
    float previous_alpha_{};
    bool initialized_{};
    bool visible_{};
    bool snapshot_{};
    bool fading_{};

public:
    static constexpr float kDuration = 0.12f;

    Visible Update(ncl_math::Vector3 pos, float time, bool snapshot, std::uint32_t seq, float move_tolerance = 0.0f);
    void Lose();
    void Reset();

private:
    Visible CollectVisible(float time) const;
};
