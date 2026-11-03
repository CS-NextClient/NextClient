#pragma once

#include <cmath>

namespace ncl_math
{
    namespace detail
    {
        // Bound x87 argument-reduction error for float results, not the instruction range.
        inline constexpr float kSinCosAccurateLimit = 8192.0f;
    } // namespace detail

    inline void SinCos(float angle, float& sine, float& cosine)
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        if (std::abs(angle) <= detail::kSinCosAccurateLimit)
        {
            float sine_value, cosine_value;
            __asm
            {
                fld angle
                fsincos
                fstp cosine_value
                fstp sine_value
            }

            sine = sine_value;
            cosine = cosine_value;

            return;
        }
#endif

        sine = std::sin(angle);
        cosine = std::cos(angle);
    }
} // namespace ncl_math
