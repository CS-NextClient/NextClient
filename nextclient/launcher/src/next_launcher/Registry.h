#pragma once

#include <string>
#include <Windows.h>
#include <iregistry.h>

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
    HKEY m_hKey;
    std::string m_context;

    char m_szBuffer[512];
};