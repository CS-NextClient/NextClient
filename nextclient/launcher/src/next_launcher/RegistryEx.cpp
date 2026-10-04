#include "RegistryEx.h"

CRegistryEx::CRegistryEx(std::string context) :
    m_context(std::move(context))
{
    m_bValid = false;
#ifdef _WIN32
    m_hKey = 0;
#endif
}

CRegistryEx::~CRegistryEx()
{
    CRegistryEx::Shutdown();
}

#ifdef _WIN32

bool CRegistryEx::Init() {
	LONG lResult;
	DWORD dwDisposition;

	lResult = RegCreateKeyExA(
		HKEY_CURRENT_USER, m_context.c_str(), 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_SET_VALUE | KEY_QUERY_VALUE, NULL,
		&m_hKey, &dwDisposition
	);
	m_bValid = lResult == ERROR_SUCCESS;

	return m_bValid;
}

void CRegistryEx::Shutdown(void) {
    if (!m_bValid)
        return;

    m_bValid = false;
    RegCloseKey(m_hKey);
}

bool CRegistryEx::Read(const std::string &key, int& output) {
	if (!m_bValid)
        return false;

	LONG lResult;
    DWORD dwType;
    DWORD dwSize = sizeof(output);

    int data;
    lResult = RegQueryValueExA(m_hKey, key.c_str(), 0, &dwType, (LPBYTE)&data, &dwSize);

    if (lResult != ERROR_SUCCESS)
        return false;

    if (dwType != REG_DWORD)
        return false;

    output = data;

    return true;
}

bool CRegistryEx::Read(const std::string &key, std::string& output) {
	if (!m_bValid)
        return false;

	output.resize(1024);

	LONG lResult;
    DWORD dwType;
    DWORD dwSize = output.size();

    lResult = RegQueryValueExA(m_hKey, key.c_str(), 0, &dwType, (LPBYTE)output.data(), &dwSize);
    output.resize(dwSize > 0 ? dwSize - 1 : 0);

    if (lResult != ERROR_SUCCESS)
        return false;

    if (dwType != REG_SZ)
        return false;

    return true;
}

bool CRegistryEx::Write(const std::string &key, int input) {
    if (!m_bValid)
        return false;

    RegSetValueExA(m_hKey, key.c_str(), 0, REG_DWORD, (LPBYTE)&input, sizeof(input));
	return true;
}

bool CRegistryEx::Write(const std::string &key, std::string input) {
    if (!m_bValid)
        return false;

    const char* str = input.c_str();

    RegSetValueExA(m_hKey, key.c_str(), 0, REG_SZ, (LPBYTE)str, strlen(str) + 1);
	return true;
}

bool CRegistryEx::DeleteKey(const std::string &key) {
	 if (!m_bValid)
        return false;

    RegDeleteKeyA(m_hKey, key.c_str());
	return true;
}

#else

bool CRegistryEx::Init() {
    m_config = std::make_unique<nitro_utils::FileConfigProvider>("registry.ini");
    m_bValid = true;

    return m_bValid;
}

void CRegistryEx::Shutdown(void) {
    if (!m_bValid)
        return;

    m_bValid = false;
    m_config = nullptr;
}

bool CRegistryEx::Read(const std::string &key, int& output) {
    if (!m_bValid)
        return false;

    std::optional<std::string> value = m_config->get_value(m_context, key);
    if (!value)
        return false;

    try
    {
        output = std::stoi(*value);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

bool CRegistryEx::Read(const std::string &key, std::string& output) {
    if (!m_bValid)
        return false;

    std::optional<std::string> value = m_config->get_value(m_context, key);
    if (!value)
        return false;

    output = *value;

    return true;
}

bool CRegistryEx::Write(const std::string &key, int input) {
    if (!m_bValid)
        return false;

    return m_config->set_value(m_context, key, std::to_string(input), true);
}

bool CRegistryEx::Write(const std::string &key, std::string input) {
    if (!m_bValid)
        return false;

    return m_config->set_value(m_context, key, input, true);
}

bool CRegistryEx::DeleteKey(const std::string &key) {
    if (!m_bValid)
        return false;

    return m_config->remove_value(m_context, key, true);
}

#endif
