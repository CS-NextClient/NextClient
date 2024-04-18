#pragma once

#include <string>
#include <Windows.h>

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
    HKEY m_hKey;
    std::string m_context;

    char m_szBuffer[512];
};