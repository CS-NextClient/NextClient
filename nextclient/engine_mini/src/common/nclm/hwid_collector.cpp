// =============================================================================
// hwid_collector.cpp  --  zSteam HWID System (client-side) v2
// =============================================================================
// Collection Strategy (in priority order):
//
//   seed1 — Motherboard UUID
//     WMI: Win32_ComputerSystemProduct.UUID (ROOT\CIMV2 — all versions)
//
//   seed2 — Boot Disk Serial Number
//     Win 8+:  WMI MSFT_Disk (Root\Microsoft\Windows\Storage) with BootFromDisk=TRUE
//     Vista/7: WMI Win32_DiskDrive + boot disk resolution via IOCTL
//     Fallback: Direct IOCTL_STORAGE_QUERY_PROPERTY (no WMI, works without WinMgmt)
//
//   If both fail: Persistent seed in HKCU\Software\NextClient\HwidSeed
//
// Final result: SHA-256(seed1 + "|" + seed2) in lowercase hex, 64 chars.
//
// v2 Improvements (absorbed from idfinder):
//   1. Uses MSFT_Disk on Win8+ — ensures BootFromDisk and correct serial
//   2. Direct IOCTL Fallback — works when WMI is disabled/corrupted
//   3. GetBootDriveNumber() — always collects the serial from the correct boot disk
// =============================================================================

#include "hwid_collector.h"

#define _WIN32_DCOM
#include <Windows.h>
#include <winioctl.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <wincrypt.h>
#include <objbase.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <array>
#include <cstdint>
#include <vector>

// Note: Library linkage (Crypt32, Advapi32, wbemuuid, etc.)
// should be handled via CMakeLists.txt: target_link_libraries(...)

namespace
{
    static std::string g_hwid_cache;
    static bool g_hwid_ready = false;

// SHA-256 result size constant
#ifndef NCLM_HWID_SIZE
#define NCLM_HWID_SIZE 64
#endif

    // -----------------------------------------------------------------------
    // SHA-256 via CryptAPI — No external dependencies
    // -----------------------------------------------------------------------
    std::string Sha256Hex(const std::string& input)
    {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;

        if (!CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            return {};

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash))
        {
            CryptReleaseContext(hProv, 0);
            return {};
        }

        if (!CryptHashData(hHash, reinterpret_cast<const BYTE*>(input.data()), static_cast<DWORD>(input.size()), 0))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return {};
        }

        DWORD hashLen = 32;
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
        return oss.str();
    }

    // -----------------------------------------------------------------------
    // Windows Version Detection (RtlGetVersion)
    // -----------------------------------------------------------------------
    struct WinVersion
    {
        DWORD major;
        DWORD minor;
        DWORD build;
    };

    WinVersion GetWinVersion()
    {
        static WinVersion cached{};
        static bool done = false;
        if (done)
            return cached;

        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll)
        {
            typedef LONG(WINAPI * RtlGetVersionFn)(PRTL_OSVERSIONINFOW);
            auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(hNtdll, "RtlGetVersion"));
            if (fn)
            {
                RTL_OSVERSIONINFOW osi{};
                osi.dwOSVersionInfoSize = sizeof(osi);
                if (fn(&osi) == 0)
                    cached = {osi.dwMajorVersion, osi.dwMinorVersion, osi.dwBuildNumber};
            }
        }
        done = true;
        return cached;
    }

    bool IsWindows8OrGreater()
    {
        auto v = GetWinVersion();
        return v.major > 6 || (v.major == 6 && v.minor >= 2);
    }

    // -----------------------------------------------------------------------
    // Hardware String Sanitization
    // -----------------------------------------------------------------------
    std::string Sanitize(const std::string& s)
    {
        const char* kInvalid[] = {
            "To be filled by O.E.M.", "Default string", "None", "00000000-0000-0000-0000-000000000000", "0000_0000_0000_"
        };

        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos)
            return {};

        std::string t = s.substr(start, end - start + 1);

        for (const char* inv : kInvalid)
            if (t.find(inv) != std::string::npos)
                return {};

        return t;
    }

    // -----------------------------------------------------------------------
    // Resolve physical boot drive number via SystemDrive
    // -----------------------------------------------------------------------
    DWORD GetBootDriveNumber()
    {
        static DWORD cached = DWORD(-1);
        static bool done = false;
        if (done)
            return cached;
        done = true;

        const char* sysDrive = getenv("SystemDrive");
        if (!sysDrive || !sysDrive[0])
            return cached;

        std::string path = "\\\\.\\";
        path += sysDrive[0];
        path += ':';

        HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return cached;

        STORAGE_DEVICE_NUMBER sdn{};
        DWORD bytes = 0;
        if (DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn, sizeof(sdn), &bytes, nullptr))
            cached = sdn.DeviceNumber;

        CloseHandle(h);
        return cached;
    }

    // -----------------------------------------------------------------------
    // WMI Helpers
    // -----------------------------------------------------------------------
    std::string BstrToStr(BSTR bstr)
    {
        if (!bstr)
            return {};
        int sz = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
        if (sz <= 1)
            return {};
        std::string out(sz - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, bstr, -1, out.data(), sz, nullptr, nullptr);
        return out;
    }

    struct WmiResult
    {
        std::string value;
        bool found;
    };

    WmiResult WmiQuery(
        const wchar_t* ns,
        const wchar_t* wmiClass,
        const wchar_t* field,
        const wchar_t* filterField = nullptr,
        const wchar_t* filterValue = nullptr
    )
    {
        WmiResult res{"", false};
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool needUninit = SUCCEEDED(hr);

        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;
        IEnumWbemClassObject* pEnum = nullptr;

        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc));
        if (FAILED(hr))
            goto cleanup;

        hr = pLoc->ConnectServer(_bstr_t(ns), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
        if (FAILED(hr))
            goto cleanup;

        CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE
        );

        {
            // Query builder: SELECT field FROM class [WHERE filter=value]
            std::wstring q = std::wstring(L"SELECT ") + field;
            if (filterField)
                q += std::wstring(L",") + filterField;
            q += L" FROM ";
            q += wmiClass;
            if (filterField && filterValue)
                q += std::wstring(L" WHERE ") + filterField + L"='" + filterValue + L"'";

            hr = pSvc->ExecQuery(
                _bstr_t(L"WQL"), _bstr_t(q.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum
            );
        }

        if (SUCCEEDED(hr))
        {
            IWbemClassObject* pObj = nullptr;
            ULONG ret = 0;
            while (pEnum->Next(5000, 1, &pObj, &ret) == WBEM_S_NO_ERROR && ret > 0)
            {
                VARIANT var;
                VariantInit(&var);
                if (SUCCEEDED(pObj->Get(field, 0, &var, nullptr, nullptr)) && var.vt == VT_BSTR && var.bstrVal)
                {
                    std::string candidate = Sanitize(BstrToStr(var.bstrVal));
                    if (!candidate.empty())
                    {
                        res.value = candidate;
                        res.found = true;
                        VariantClear(&var);
                        pObj->Release();
                        break;
                    }
                }
                VariantClear(&var);
                pObj->Release();
            }
        }

    cleanup:
        if (pEnum)
            pEnum->Release();
        if (pSvc)
            pSvc->Release();
        if (pLoc)
            pLoc->Release();
        if (needUninit)
            CoUninitialize();
        return res;
    }

    // -----------------------------------------------------------------------
    // HWID Seeds Collection logic
    // -----------------------------------------------------------------------
    std::string CollectMotherboardUUID()
    {
        auto r = WmiQuery(L"ROOT\\CIMV2", L"Win32_ComputerSystemProduct", L"UUID");
        return r.found ? r.value : std::string{};
    }

    std::string CollectBootDiskSerial_IOCTL()
    {
        // Direct IOCTL Path: Bypasses WMI service dependencies
        DWORD bootNum = GetBootDriveNumber();
        const DWORD kMaxDrives = 8;
        DWORD order[kMaxDrives];
        DWORD count = 0;

        // Prioritize the detected boot drive
        if (bootNum != DWORD(-1) && bootNum < kMaxDrives)
            order[count++] = bootNum;
        for (DWORD i = 0; i < kMaxDrives; i++)
            if (i != bootNum)
                order[count++] = i;

        for (DWORD di = 0; di < count; di++)
        {
            std::string path = "\\\\.\\PhysicalDrive" + std::to_string(order[di]);
            HANDLE h = CreateFileA(
                path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr
            );
            if (h == INVALID_HANDLE_VALUE)
                h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

            if (h == INVALID_HANDLE_VALUE)
                continue;

            DISK_GEOMETRY dg{};
            DWORD bytes = 0;
            if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &dg, sizeof(dg), &bytes, nullptr) ||
                dg.MediaType != FixedMedia)
            {
                CloseHandle(h);
                continue;
            }

            STORAGE_PROPERTY_QUERY spq{};
            spq.PropertyId = StorageDeviceProperty;
            spq.QueryType = PropertyStandardQuery;

            STORAGE_DESCRIPTOR_HEADER sdh{};
            if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), &sdh, sizeof(sdh), &bytes, nullptr) || sdh.Size == 0)
            {
                CloseHandle(h);
                continue;
            }

            std::vector<BYTE> buf(sdh.Size);
            if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), buf.data(), sdh.Size, &bytes, nullptr))
            {
                auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buf.data());
                // Filter for fixed internal drives only (SATA, ATA, NVMe, RAID)
                if (desc->SerialNumberOffset && (desc->BusType == BusTypeSata || desc->BusType == BusTypeAta ||
                                                 desc->BusType == BusTypeNvme || desc->BusType == BusTypeRAID))
                {
                    std::string serial(reinterpret_cast<const char*>(buf.data() + desc->SerialNumberOffset));

                    // ATA/SATA requires byte swapping for correct serial reading
                    if (desc->BusType == BusTypeSata || desc->BusType == BusTypeAta)
                        for (size_t i = 0; i + 1 < serial.size(); i += 2)
                            std::swap(serial[i], serial[i + 1]);

                    std::string s = Sanitize(serial);
                    if (!s.empty())
                    {
                        CloseHandle(h);
                        return s;
                    }
                }
            }
            CloseHandle(h);
        }
        return {};
    }

    std::string CollectBootDiskSerial()
    {
        // Path A: Win 8+ via MSFT_Disk (Storage Namespace)
        if (IsWindows8OrGreater())
        {
            auto r = WmiQuery(L"Root\\Microsoft\\Windows\\Storage", L"MSFT_Disk", L"SerialNumber", L"BootFromDisk", L"TRUE");
            if (r.found)
                return r.value;
        }

        // Path B: Vista/7 via Win32_DiskDrive cross-referenced with boot index
        DWORD bootNum = GetBootDriveNumber();
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool needUninit = SUCCEEDED(hr);
        std::string result;

        if (needUninit)
        {
            IWbemLocator* pLoc = nullptr;
            IWbemServices* pSvc = nullptr;
            IEnumWbemClassObject* pEnum = nullptr;

            if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc)))
            {
                if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc)))
                {
                    CoSetProxyBlanket(
                        pSvc,
                        RPC_C_AUTHN_WINNT,
                        RPC_C_AUTHZ_NONE,
                        nullptr,
                        RPC_C_AUTHN_LEVEL_CALL,
                        RPC_C_IMP_LEVEL_IMPERSONATE,
                        nullptr,
                        EOAC_NONE
                    );
                    if (SUCCEEDED(pSvc->ExecQuery(
                            _bstr_t(L"WQL"),
                            _bstr_t(L"SELECT Index,SerialNumber FROM Win32_DiskDrive WHERE MediaType='Fixed hard disk media'"),
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                            nullptr,
                            &pEnum
                        )))
                    {
                        IWbemClassObject* pObj = nullptr;
                        ULONG ret = 0;
                        while (pEnum->Next(5000, 1, &pObj, &ret) == WBEM_S_NO_ERROR && ret > 0)
                        {
                            VARIANT vIdx, vSerial;
                            VariantInit(&vIdx);
                            VariantInit(&vSerial);

                            // Corrected precedence: Check Get success and Type together
                            if (SUCCEEDED(pObj->Get(L"Index", 0, &vIdx, nullptr, nullptr)) && (vIdx.vt == VT_I4 || vIdx.vt == VT_UI4))
                            {
                                DWORD idx = (vIdx.vt == VT_UI4) ? vIdx.uintVal : (DWORD)vIdx.intVal;
                                if (bootNum == DWORD(-1) || idx == bootNum)
                                {
                                    if (SUCCEEDED(pObj->Get(L"SerialNumber", 0, &vSerial, nullptr, nullptr)) && vSerial.vt == VT_BSTR &&
                                        vSerial.bstrVal)
                                    {
                                        result = Sanitize(BstrToStr(vSerial.bstrVal));
                                    }
                                }
                            }
                            VariantClear(&vIdx);
                            VariantClear(&vSerial);
                            pObj->Release();
                            if (!result.empty())
                                break;
                        }
                        pEnum->Release();
                    }
                    pSvc->Release();
                }
                pLoc->Release();
            }
            CoUninitialize();
        }

        if (!result.empty())
            return result;

        // Path C: Final fallback using IOCTL directly (No WMI dependency)
        return CollectBootDiskSerial_IOCTL();
    }

    // -----------------------------------------------------------------------
    // Persistent Fallback: Registry-based seed
    // -----------------------------------------------------------------------
    std::string GetOrCreateRegistryFallback()
    {
        const char* keyPath = "Software\\Valve\\Half-Life\\nextclient\\global";
        const char* valueName = "HwidSeed";
        HKEY hKey = nullptr;
        char buf[64]{};
        DWORD bufSize = sizeof(buf);

        if (RegCreateKeyExA(
                HKEY_CURRENT_USER, keyPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, nullptr, &hKey, nullptr
            ) != ERROR_SUCCESS)
            return "fallback_no_registry";

        if (RegQueryValueExA(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &bufSize) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return std::string(buf);
        }

        // Generate a random seed with improved entropy using CryptGenRandom
        HCRYPTPROV hProv = 0;
        ULONGLONG seedVal = GetTickCount64();
        if (CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        {
            CryptGenRandom(hProv, sizeof(seedVal), reinterpret_cast<BYTE*>(&seedVal));
            CryptReleaseContext(hProv, 0);
        }

        std::ostringstream oss;
        oss << std::hex << seedVal;
        std::string seed = oss.str();

        RegSetValueExA(hKey, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(seed.c_str()), static_cast<DWORD>(seed.size() + 1));
        RegCloseKey(hKey);
        return seed;
    }
} // namespace

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------
namespace hwid
{
    std::string Collect()
    {
        if (g_hwid_ready)
            return g_hwid_cache;

        std::string s1 = CollectMotherboardUUID(); // Hardware Seed 1
        std::string s2 = CollectBootDiskSerial(); // Hardware Seed 2

        // If both hardware identifiers fail, use the registry-persistent fallback
        if (s1.empty() && s2.empty())
            s1 = GetOrCreateRegistryFallback();

        std::string combined = s1 + "|" + s2;
        std::string hwid = Sha256Hex(combined);

        // Ensure the hash result matches the expected size constant
        if (hwid.size() == NCLM_HWID_SIZE)
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
} // namespace hwid
