#pragma once

#include <cctype>
#include <cstdlib>

// The crosshair the HUD draws and the settings preview mirrors: its shapes, its sizes,
// the dynamic spread of the rifle the preview stands in for, and the arithmetic both draw
// it by.
namespace crosshair
{
    // cl_crosshair_type values
    inline constexpr int kTypeCross = 0;
    inline constexpr int kTypeT = 1;
    inline constexpr int kTypeCircle = 2;
    inline constexpr int kTypeDot = 3;
    inline constexpr int kTypeCount = 4;

    // A cl_crosshair_size value, by the name the cvar takes and the number it takes as
    // well; scale_base is the screen width the bar lengths are authored for, 0 for the
    // width-dependent auto.
    struct Size
    {
        const char* name;
        int scale_base;
    };

    inline constexpr Size kSizes[] = {
        {"auto", 0},
        {"small", 1024},
        {"medium", 800},
        {"large", 640},
        {"extra_small", 1400},
    };

    inline constexpr int kSizeCount = 5;
    inline constexpr int kSizeAuto = 0;

    inline bool SameName(const char* a, const char* b)
    {
        for (; *a != '\0' && *b != '\0'; a++, b++)
        {
            if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
                return false;
        }

        return *a == *b;
    }

    // The index into kSizes of a cvar value, by name or by number; anything else is auto.
    inline int SizeIndex(const char* value)
    {
        if (value == nullptr)
            return kSizeAuto;

        if (std::isdigit(static_cast<unsigned char>(value[0])))
        {
            int index = std::atoi(value);

            return index >= 0 && index < kSizeCount ? index : kSizeAuto;
        }

        for (int i = 0; i < kSizeCount; i++)
        {
            if (SameName(value, kSizes[i].name))
                return i;
        }

        return kSizeAuto;
    }

    // The width the bar lengths of a size are authored for, on a screen of the given width.
    inline int ScaleBase(int size_index, int screen_wide)
    {
        int base = kSizes[size_index].scale_base;

        if (base != 0)
            return base;

        if (screen_wide >= 1024)
            return 640;

        if (screen_wide >= 800)
            return 800;

        return 1024;
    }

    // Dynamic spread of the rifles: the gap at rest, the speed past which running widens
    // it, and by how much.
    inline constexpr int kRifleGap = 4;
    inline constexpr float kRifleRunSpeed = 140.0f;
    inline constexpr float kRunSpread = 1.5f;

    // Closes the gap toward the resting one over a frame, fast while wide and slower near rest.
    inline float Decay(float distance, float frametime)
    {
        return distance - (1.3f * distance + 10.0f) * frametime;
    }

    // Bar length in pixels at the authored width, for a gap that has opened past gap.
    inline int BarSize(float distance, int gap)
    {
        return static_cast<int>((distance - gap) * 0.5f + 5);
    }

    inline float ScaledDistance(float distance, int screen_wide, int scale_base)
    {
        return distance * static_cast<float>(screen_wide) / static_cast<float>(scale_base);
    }

    inline int ScaledBarSize(int bar_size, int screen_wide, int scale_base)
    {
        return screen_wide * bar_size / scale_base;
    }
}
