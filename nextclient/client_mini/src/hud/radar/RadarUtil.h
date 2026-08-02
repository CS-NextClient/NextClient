#pragma once

#include <cstring>
#include <filesystem>
#include <span>

// Anchors a relative name at game_dir and makes it absolute: the process working
// directory is not game_dir under Steam or the launcher.
inline std::filesystem::path RadarUtil_AnchorPath(const char* game_dir, const char* name)
{
    std::filesystem::path path(name);

    if (!path.is_absolute() && game_dir && game_dir[0])
    {
        path = std::filesystem::path(game_dir) / path;
    }

    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec);

    std::filesystem::path result = ec ? path : abs;
    return result;
}

// "maps/de_dust2.bsp" -> "de_dust2"; writes at most out.size() - 1 characters plus a null.
inline void RadarUtil_ExtractMapName(const char* level_name, std::span<char> out)
{
    const char* begin = level_name;
    const char* slash = std::strrchr(level_name, '/');

    if (slash)
    {
        begin = slash + 1;
    }

    int i = 0;
    while (begin[i] && begin[i] != '.' && i < (int)out.size() - 1)
    {
        out[i] = begin[i];
        i++;
    }
    out[i] = '\0';
}

namespace radar_util_detail
{
    inline char ToAB(char c)
    {
        if (c == 'a' || c == 'A')
        {
            return 'A';
        }

        if (c == 'b' || c == 'B')
        {
            return 'B';
        }

        return 0;
    }

    inline bool IsAlpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }
} // namespace radar_util_detail

// Bomb-target names encode the site letter as "tgt_a", "a_crates", "targeta" or "a".
// Returns 'A'/'B', or 0 when none of those boundaries match.
inline char RadarUtil_DetectSiteLetter(const char* s)
{
    using namespace radar_util_detail;

    int n = (int)std::strlen(s);
    if (n == 0)
    {
        return 0;
    }

    if (n >= 2 && s[n - 2] == '_')
    {
        if (char r = ToAB(s[n - 1]))
        {
            return r;
        }
    }

    if (n >= 2 && s[1] == '_')
    {
        if (char r = ToAB(s[0]))
        {
            return r;
        }
    }

    if (char r = ToAB(s[n - 1]))
    {
        return r;
    }

    if (n == 1 || !IsAlpha(s[1])) // a leading 'a'/'b' only counts when it is not a word
    {
        if (char r = ToAB(s[0]))
        {
            return r;
        }
    }

    return 0;
}
