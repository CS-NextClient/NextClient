#pragma once

#include <cstring>
#include <filesystem>

// Anchors a relative file name at game_dir (where configs/demos/screenshots live),
// normalized to an absolute path, so the location does not depend on the process
// working directory (Steam / the launcher can start with an unexpected one). An
// absolute name is used as given.
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

// "maps/de_dust2.bsp" -> "de_dust2"
inline void RadarUtil_ExtractMapName(const char* level_name, char* out, int out_size)
{
    const char* begin = level_name;
    const char* slash = std::strrchr(level_name, '/');

    if (slash)
    {
        begin = slash + 1;
    }

    int i = 0;
    while (begin[i] && begin[i] != '.' && i < out_size - 1)
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

// Many maps encode the bomb-site letter in the bomb-target name in various ways:
// "tgt_a"/"bomb_a" (suffix), "a_crates"/"b_crates" (prefix), "targeta"/"cratesa"
// (trailing), "a"/"b" (whole). Detect 'A'/'B' at those boundaries; 0 when none match.
inline char RadarUtil_DetectSiteLetter(const char* s)
{
    using namespace radar_util_detail;

    int n = (int)std::strlen(s);
    if (n == 0)
    {
        return 0;
    }

    if (n >= 2 && s[n - 2] == '_') // "..._a" / "..._b"
    {
        if (char r = ToAB(s[n - 1]))
        {
            return r;
        }
    }

    if (n >= 2 && s[1] == '_') // "a_..." / "b_..."
    {
        if (char r = ToAB(s[0]))
        {
            return r;
        }
    }

    if (char r = ToAB(s[n - 1])) // "...a" / "...b"
    {
        return r;
    }

    if (n == 1 || !IsAlpha(s[1])) // "a..." / "b..." (not a word)
    {
        if (char r = ToAB(s[0]))
        {
            return r;
        }
    }

    return 0;
}
