#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

#include "graphics/render/StudioPose.h"

inline render::StudioPose CapturePose(
    std::initializer_list<std::pair<size_t, float>> components = {},
    const render::StudioPose& source = {}
)
{
    std::array<float, render::kStudioBoneFloats> values{};
    if (source.data() != nullptr)
    {
        std::copy_n(source.data(), values.size(), values.data());
    }

    for (const std::pair<size_t, float>& component : components)
    {
        values[component.first] = component.second;
    }

    render::StudioPosePool pool;

    return pool.Capture(values.data());
}
