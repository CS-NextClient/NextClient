#include "engine.h"
#include "color_scheme.h"

#include <FileSystem.h>

#include <string>

namespace
{
    // Mirror of GameUI OptionsSubMiscellaneous constants (kept in sync)
    constexpr char kSettingsFile[] = "MiscellaneousSettings.vdf";
    constexpr char kSettingsPathId[] = "GAMECONFIG";
    constexpr char kSchemeKey[] = "Scheme";
    constexpr char kDefaultScheme[] = "TrackerScheme_ClassicPlus.res";
    constexpr char kSchemesDir[] = "resource/schemes";
    constexpr char kBaseScheme[] = "resource/trackerscheme.res";

    std::string ReadSelectedScheme()
    {
        FileHandle_t file = g_pFileSystem->Open(kSettingsFile, "rb", kSettingsPathId);
        if (!file)
        {
            return {};
        }

        unsigned int size = g_pFileSystem->Size(file);
        std::string buffer(size, '\0');
        int read = g_pFileSystem->Read(buffer.data(), size, file);
        g_pFileSystem->Close(file);
        if (read <= 0)
        {
            return {};
        }
        buffer.resize(read);

        std::string key = std::string("\"") + kSchemeKey + "\"";
        size_t key_pos = buffer.find(key);
        if (key_pos == std::string::npos)
        {
            return {};
        }

        size_t value_open = buffer.find('"', key_pos + key.size());
        if (value_open == std::string::npos)
        {
            return {};
        }

        size_t value_close = buffer.find('"', value_open + 1);
        if (value_close == std::string::npos)
        {
            return {};
        }

        return buffer.substr(value_open + 1, value_close - value_open - 1);
    }
}

void ColorScheme_ApplyAlias()
{
    if (!g_pFileSystem || !g_pFileSystemNext)
    {
        return;
    }

    std::string scheme = ReadSelectedScheme();

    if (scheme.empty() || scheme == kDefaultScheme)
    {
        return;
    }

    std::string scheme_path = std::string(kSchemesDir) + "/" + scheme;
    if (!g_pFileSystem->FileExists(scheme_path.c_str()))
    {
        return;
    }

    g_pFileSystemNext->SetPathAlias(scheme_path.c_str(), kBaseScheme);
}
