#pragma once

#include <string>
#include <iregistry.h>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <memory>
    #include <nitro_utils/config/FileConfigProvider.h>
#endif

class CRegistry : public IRegistry
{
public:
    CRegistry(std::string  context);
    virtual ~CRegistry(void);

public:
    void Init() override;
    void Shutdown(void) override;
    int ReadInt(const char* key, int defaultValue = 0) override;
    void WriteInt(const char* key, int value) override;
    const char* ReadString(const char* key, const char* defaultValue = NULL) override;
    void WriteString(const char* key, const char* value) override;

    void DeleteKey(const char* key);

private:
    bool m_bValid;
    std::string m_context;

#ifdef _WIN32
    HKEY m_hKey;
    char m_szBuffer[512]{};
#else
    std::unique_ptr<nitro_utils::FileConfigProvider> m_config;
    std::string m_szBuffer;
#endif
};
