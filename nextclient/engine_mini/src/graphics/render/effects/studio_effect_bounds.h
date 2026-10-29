#pragma once

#include <array>

namespace render
{
    struct StudioDrawPacket;
    struct StudioGpuModel;

    namespace effects
    {
        using Rect_t = std::array<int, 4>;

        Rect_t ProjectBounds(const StudioGpuModel& model, const StudioDrawPacket& packet, float radius);
    } // namespace effects
} // namespace render
