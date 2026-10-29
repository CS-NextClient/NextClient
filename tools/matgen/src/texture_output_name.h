#pragma once

#include <algorithm>
#include <locale>
#include <set>
#include <string>
#include <vector>

namespace matgen
{
    inline std::string TextureOutputName(const std::string& texture_name)
    {
        std::string result;

        for (char c : texture_name)
        {
            bool plain = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
            result.push_back(plain ? c : '_');
        }

        return result;
    }

    inline std::string CheckTextureOutputNames(const std::vector<std::string>& filenames)
    {
        std::set<std::string> destinations;

        for (const std::string& filename : filenames)
        {
            if (filename.empty())
            {
                continue;
            }

            std::string key = filename;
            std::transform(key.begin(), key.end(), key.begin(), [](char c) {
                return std::tolower(c, std::locale::classic());
            });

            if (!destinations.insert(key).second)
            {
                return "texture output collision: " + filename;
            }
        }

        return {};
    }
} // namespace matgen
