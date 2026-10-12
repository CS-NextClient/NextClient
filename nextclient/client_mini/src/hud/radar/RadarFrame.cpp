#include "RadarFrame.h"

namespace
{
    constexpr float kClockCorrectionTolerance = 0.01f;
} // namespace

float RadarFrame_NormalizeTime(float time, float previous_time)
{
    // The client clock can regress slightly during packet timing corrections.
    float regression = previous_time - time;
    return regression > 0.0f && regression <= kClockCorrectionTolerance ? previous_time : time;
}
