#pragma once

#include <string>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <memory>
    #include <nitro_utils/config/FileConfigProvider.h>
#endif

class CRegistryEx
{
public:
    CRegistryEx(std::string context);
    virtual ~CRegistryEx(void);

public:
    bool Init();
    void Shutdown(void);

    bool Read(const std::string &key, int &output);
	bool Read(const std::string &key, std::string &output);

    bool Write(const std::string &key, int input);
	bool Write(const std::string &key, std::string input);

    bool DeleteKey(const std::string &key);

private:
    bool m_bValid;
    std::string m_context;

#ifdef _WIN32
    HKEY m_hKey;
    char m_szBuffer[512];
#else
    std::unique_ptr<nitro_utils::FileConfigProvider> m_config;
#endif
};
