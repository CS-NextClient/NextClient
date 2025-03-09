#pragma once

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
