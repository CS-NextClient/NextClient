#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include "hwid_collector.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <wincrypt.h>
#include <objbase.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <array>

namespace
{
    static std::string g_hwid_cache;
    static bool        g_hwid_ready = false;

    std::string Sha256Hex(const std::string& input)
    {
        HCRYPTPROV hProv  = 0;
        HCRYPTHASH hHash  = 0;
        std::string result;

        if (!CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            return {};

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
        {
            CryptReleaseContext(hProv, 0);
            return {};
        }

        if (!CryptHashData(hHash,
                           reinterpret_cast<const BYTE*>(input.data()),
                           static_cast<DWORD>(input.size()), 0))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return {};
        }

        DWORD hashLen = 32; // SHA-256 = 256 bits = 32 bytes
        std::array<BYTE, 32> hashBytes{};
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hashBytes.data(), &hashLen, 0))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return {};
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (BYTE b : hashBytes)
            oss << std::setw(2) << static_cast<int>(b);

        return oss.str(); // 64 chars hex
    }

    std::string WmiQueryFirstField(const wchar_t* wmiClass, const wchar_t* field)
    {
        std::string result;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool needUninit = SUCCEEDED(hr);

        IWbemLocator*  pLoc  = nullptr;
        IWbemServices* pSvc  = nullptr;
        IEnumWbemClassObject* pEnum = nullptr;
        IWbemClassObject*     pObj  = nullptr;

        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc));
        if (FAILED(hr)) goto cleanup;

        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr,
                                 0, nullptr, nullptr, &pSvc);
        if (FAILED(hr)) goto cleanup;

        hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                               RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                               nullptr, EOAC_NONE);
        if (FAILED(hr)) goto cleanup;

        {
            std::wstring query = std::wstring(L"SELECT ") + field +
                                 L" FROM " + wmiClass;
            hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query.c_str()),
                                 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                 nullptr, &pEnum);
        }
        if (FAILED(hr)) goto cleanup;

        {
            ULONG ret = 0;
            hr = pEnum->Next(WBEM_INFINITE, 1, &pObj, &ret);
            if (FAILED(hr) || ret == 0) goto cleanup;

            VARIANT var;
            VariantInit(&var);
            hr = pObj->Get(field, 0, &var, nullptr, nullptr);
            if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal)
            {
                int sz = WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1,
                                             nullptr, 0, nullptr, nullptr);
                if (sz > 1)
                {
                    result.resize(sz - 1);
                    WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1,
                                       result.data(), sz, nullptr, nullptr);
                }
            }
            VariantClear(&var);
            pObj->Release();
        }

    cleanup:
        if (pEnum) pEnum->Release();
        if (pSvc)  pSvc->Release();
        if (pLoc)  pLoc->Release();
        if (needUninit) CoUninitialize();

        return result;
    }

    std::string GetOrCreateRegistryFallback()
    {
        const char* keyPath = "Software\\NextClient";
        const char* valueName = "HwidSeed";

        HKEY hKey = nullptr;
        char buf[64]{};
        DWORD bufSize = sizeof(buf);
        DWORD dwType  = REG_SZ;

        if (RegCreateKeyExA(HKEY_CURRENT_USER, keyPath, 0, nullptr,
                            REG_OPTION_NON_VOLATILE,
                            KEY_READ | KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
            return "fallback_no_registry";

        LONG res = RegQueryValueExA(hKey, valueName, nullptr, &dwType,
                                    reinterpret_cast<LPBYTE>(buf), &bufSize);
        if (res == ERROR_SUCCESS && bufSize > 1)
        {
            RegCloseKey(hKey);
            return std::string(buf, bufSize - 1);
        }

        ULONGLONG tick = GetTickCount64();
        DWORD     pid  = GetCurrentProcessId();
        std::ostringstream oss;
        oss << std::hex << tick << pid;
        std::string seed = oss.str();

        RegSetValueExA(hKey, valueName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(seed.c_str()),
                       static_cast<DWORD>(seed.size() + 1));
        RegCloseKey(hKey);
        return seed;
    }

    std::string Sanitize(const std::string& s)
    {
        const char* kInvalid[] = {
            "To be filled by O.E.M.", "Default string", "None",
            "00000000-0000-0000-0000-000000000000", ""
        };

        // trim
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end   = s.find_last_not_of(" \t\r\n");
        std::string trimmed = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);

        for (const char* inv : kInvalid)
        {
            if (trimmed == inv)
                return {};
        }
        return trimmed;
    }

} // anonymous namespace

namespace hwid
{
    std::string Collect()
    {
        if (g_hwid_ready)
            return g_hwid_cache;

        std::string seed1 = Sanitize(
            WmiQueryFirstField(L"Win32_ComputerSystemProduct", L"UUID"));

        std::string seed2 = Sanitize(
            WmiQueryFirstField(L"Win32_DiskDrive", L"SerialNumber"));

        if (seed1.empty() && seed2.empty())
            seed1 = GetOrCreateRegistryFallback();

        std::string combined = seed1 + "|" + seed2;
        std::string hwid = Sha256Hex(combined);

        if (hwid.size() == 64)
        {
            g_hwid_cache = hwid;
            g_hwid_ready = true;
        }

        return g_hwid_cache;
    }

    bool IsReady()
    {
        return g_hwid_ready;
    }

    void Reset()
    {
        g_hwid_cache.clear();
        g_hwid_ready = false;
    }

}
