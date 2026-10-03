#include "Registry.h"
#include <utility>

CRegistry::CRegistry(std::string context) :
    m_context(std::move(context))
{
    m_bValid = false;
#ifdef _WIN32
    m_hKey = 0;
#endif
}

CRegistry::~CRegistry()
{
    CRegistry::Shutdown();
}

#ifdef _WIN32

int CRegistry::ReadInt(const char* key, int defaultValue)
{
    LONG lResult;
    DWORD dwType;
    DWORD dwSize;

    int value;

    if (!m_bValid)
        return defaultValue;

    dwSize = sizeof(DWORD);
    lResult = RegQueryValueExA(m_hKey, key, 0, &dwType, (LPBYTE)&value, &dwSize);

    if (lResult != ERROR_SUCCESS)
        return defaultValue;

    if (dwType != REG_DWORD)
        return defaultValue;

    return value;
}

void CRegistry::WriteInt(const char* key, int value)
{
    DWORD dwSize;

    if (!m_bValid)
        return;

    dwSize = sizeof(DWORD);
    RegSetValueExA(m_hKey, key, 0, REG_DWORD, (LPBYTE)&value, dwSize);
}

const char* CRegistry::ReadString(const char* key, const char* defaultValue)
{
    LONG lResult;
    DWORD dwType;
    DWORD dwSize = sizeof(m_szBuffer);

    if (!m_bValid)
        return defaultValue;

    lResult = RegQueryValueExA(m_hKey, key, 0, &dwType, (unsigned char*)m_szBuffer, &dwSize);

    if (lResult != ERROR_SUCCESS)
        return defaultValue;

    if (dwType != REG_SZ)
        return defaultValue;

    return m_szBuffer;
}

void CRegistry::WriteString(const char* key, const char* value)
{
    DWORD dwSize;

    if (!m_bValid)
        return;

    dwSize = (DWORD)(strlen(value) + 1);
    RegSetValueExA(m_hKey, key, 0, REG_SZ, (LPBYTE)value, dwSize);
}

void CRegistry::Init()
{
    LONG lResult;
    DWORD dwDisposition;

    lResult = RegCreateKeyExA(HKEY_CURRENT_USER, m_context.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL, &m_hKey, &dwDisposition);
    if (lResult != ERROR_SUCCESS)
    {
        m_bValid = false;
        return;
    }

    m_bValid = true;
}

void CRegistry::Shutdown(void)
{
    if (!m_bValid)
        return;

    m_bValid = false;
    RegCloseKey(m_hKey);
}

void CRegistry::DeleteKey(const char *key)
{
    if (!m_bValid)
        return;

    RegDeleteKeyA(m_hKey, key);
}

#else

int CRegistry::ReadInt(const char* key, int defaultValue)
{
    if (!m_bValid)
        return defaultValue;

    std::optional<std::string> value = m_config->get_value(m_context, key);
    if (!value)
        return defaultValue;

    try
    {
        return std::stoi(*value);
    }
    catch (...)
    {
        return defaultValue;
    }
}

void CRegistry::WriteInt(const char* key, int value)
{
    if (!m_bValid)
        return;

    m_config->set_value(m_context, key, std::to_string(value), true);
}

const char* CRegistry::ReadString(const char* key, const char* defaultValue)
{
    if (!m_bValid)
        return defaultValue;

    std::optional<std::string> value = m_config->get_value(m_context, key);
    if (!value)
        return defaultValue;

    m_szBuffer = *value;
    return m_szBuffer.c_str();
}

void CRegistry::WriteString(const char* key, const char* value)
{
    if (!m_bValid)
        return;

    m_config->set_value(m_context, key, value, true);
}

void CRegistry::Init()
{
    m_config = std::make_unique<nitro_utils::FileConfigProvider>("registry.ini");
    m_bValid = true;
}

void CRegistry::Shutdown(void)
{
    if (!m_bValid)
        return;

    m_bValid = false;
    m_config = nullptr;
}

void CRegistry::DeleteKey(const char* key)
{
    // unused - no callers anywhere in the codebase
}

#endif
