#pragma once

#include <cstdio>
#include <string>
#include <sstream>

struct NextClientVersion
{
    int major{};
    int minor{};
    int patch{};
    char prerelease[128]{};
};

inline std::string BuildNextClientVersionString(const NextClientVersion& version)
{
    std::stringstream ss;
    ss << version.major << "." << version.minor << "." << version.patch;

    if (version.prerelease[0] != '\0')
    {
        ss << "-" << version.prerelease;
    }

    return ss.str();
}

inline bool ParseNextClientVersion(const std::string& str, NextClientVersion& out)
{
    NextClientVersion version{};
    if (std::sscanf(str.c_str(), "%d.%d.%d", &version.major, &version.minor, &version.patch) != 3)
    {
        return false;
    }

    if (version.major < 0 || version.minor < 0 || version.patch < 0)
    {
        return false;
    }

    out = version;
    return true;
}

inline bool operator>=(const NextClientVersion& lhs, const NextClientVersion& rhs)
{
    if (lhs.major != rhs.major)
    {
        return lhs.major > rhs.major;
    }

    if (lhs.minor != rhs.minor)
    {
        return lhs.minor > rhs.minor;
    }

    return lhs.patch >= rhs.patch;
}
