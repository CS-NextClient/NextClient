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
//   3. getSystemBootDriveNumber() — always collects the serial from the correct boot disk
// =============================================================================

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include "hwid_collector.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------
namespace
{
    static std::string g_hwid_cache;
    static bool g_hwid_ready = false;

    // -----------------------------------------------------------------------
    // SHA-256 via CryptAPI — no external dependencies
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
    // Windows Version Detection (idfinder: RtlGetVersion)
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
            "To be filled by O.E.M.", "Default string", "None", "00000000-0000-0000-0000-000000000000", "0000_0000_0000_", ""
        };

        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos)
            return {};
        std::string t = s.substr(start, end - start + 1);

        for (const char* inv : kInvalid)
            if (t.find(inv) != std::string::npos || t == inv)
                return {};

        return t;
    }

    // -----------------------------------------------------------------------
    // IMPROVEMENT 3: Resolve physical boot drive number
    // idfinder technique: getenv("SystemDrive") ->
    //   CreateFile -> IOCTL_STORAGE_GET_DEVICE_NUMBER
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
    // Generic WMI helper — initializes COM, executes query, returns string.
    // -----------------------------------------------------------------------
    // Converts BSTR -> std::string
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

    // Connects to WMI namespace and executes a simple SELECT.
    // Returns the value of the `field` from the first row that passes
    // through the optional `filterField`/`filterValue`.
    WmiResult WmiQuery(
        const wchar_t* ns,
        const wchar_t* wmiClass,
        const wchar_t* field,
        const wchar_t* filterField = nullptr, // e.g.: L"BootFromDisk"
        const wchar_t* filterValue = nullptr
    ) // e.g.: L"TRUE"
    {
        WmiResult res{};

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

        hr = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE
        );
        if (FAILED(hr))
            goto cleanup;

        {
            // Build query: SELECT field [,filterField] FROM wmiClass [WHERE filterField=filterValue]
            std::wstring q = std::wstring(L"SELECT ") + field;
            if (filterField)
            {
                q += L",";
                q += filterField;
            }
            q += L" FROM ";
            q += wmiClass;
            if (filterField && filterValue)
            {
                q += L" WHERE ";
                q += filterField;
                q += L"=";
                q += filterValue;
            }

            hr = pSvc->ExecQuery(
                _bstr_t(L"WQL"), _bstr_t(q.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum
            );
        }
        if (FAILED(hr))
            goto cleanup;

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
    // seed1: Motherboard UUID
    // -----------------------------------------------------------------------
    std::string CollectMotherboardUUID()
    {
        auto r = WmiQuery(L"ROOT\\CIMV2", L"Win32_ComputerSystemProduct", L"UUID");
        return r.found ? r.value : std::string{};
    }

    // -----------------------------------------------------------------------
    // IMPROVEMENTS 1+2: seed2 — Boot Disk Serial
    //
    // Path A (Win 8+): MSFT_Disk with BootFromDisk=TRUE
    //   Namespace: Root\Microsoft\Windows\Storage
    //   Advantage: Native BootFromDisk field, already normalized serial
    //
    // Path B (Vista/7): Win32_DiskDrive + IOCTL resolution
    //   Iterates WMI disks and matches against GetBootDriveNumber()
    //
    // Path C (no WMI): Direct IOCTL_STORAGE_QUERY_PROPERTY
    //   Opens \\.\PhysicalDriveN, reads STORAGE_DEVICE_DESCRIPTOR
    //   Works even if WMI is disabled (antivirus, corporate domain)
    // -----------------------------------------------------------------------
    std::string CollectBootDiskSerial_MSFT()
    {
        // Path A: Win8+ — MSFT_Disk with BootFromDisk=TRUE
        auto r = WmiQuery(L"Root\\Microsoft\\Windows\\Storage", L"MSFT_Disk", L"SerialNumber", L"BootFromDisk", L"TRUE");
        return r.found ? r.value : std::string{};
    }

    std::string CollectBootDiskSerial_Win32()
    {
        // Path B: Enumerate Win32_DiskDrive and cross-reference with GetBootDriveNumber()
        DWORD bootNum = GetBootDriveNumber();

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool needUninit = SUCCEEDED(hr);

        std::string result;
        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;
        IEnumWbemClassObject* pEnum = nullptr;

        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc));
        if (FAILED(hr))
            goto cleanup;

        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
        if (FAILED(hr))
            goto cleanup;

        hr = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE
        );
        if (FAILED(hr))
            goto cleanup;

        hr = pSvc->ExecQuery(
            _bstr_t(L"WQL"),
            _bstr_t(L"SELECT Index,SerialNumber FROM Win32_DiskDrive "
                    L"WHERE MediaType='Fixed hard disk media'"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &pEnum
        );
        if (FAILED(hr))
            goto cleanup;

        {
            IWbemClassObject* pObj = nullptr;
            ULONG ret = 0;
            while (pEnum->Next(5000, 1, &pObj, &ret) == WBEM_S_NO_ERROR && ret > 0)
            {
                VARIANT vIdx, vSerial;
                VariantInit(&vIdx);
                VariantInit(&vSerial);

                bool ok = false;
                if (SUCCEEDED(pObj->Get(L"Index", 0, &vIdx, nullptr, nullptr)) && vIdx.vt == VT_I4 || vIdx.vt == VT_UI4)
                {
                    DWORD idx = (vIdx.vt == VT_UI4) ? vIdx.uintVal : (DWORD)vIdx.intVal;

                    // If we successfully resolved the boot drive, filter it; otherwise take the first
                    if (bootNum == DWORD(-1) || idx == bootNum)
                    {
                        if (SUCCEEDED(pObj->Get(L"SerialNumber", 0, &vSerial, nullptr, nullptr)) && vSerial.vt == VT_BSTR &&
                            vSerial.bstrVal)
                        {
                            std::string s = Sanitize(BstrToStr(vSerial.bstrVal));
                            if (!s.empty())
                            {
                                result = s;
                                ok = true;
                            }
                        }
                    }
                }

                VariantClear(&vIdx);
                VariantClear(&vSerial);
                pObj->Release();
                if (ok)
                    break;
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
        return result;
    }

    std::string CollectBootDiskSerial_IOCTL()
    {
        // Path C: Direct IOCTL — no WMI dependency
        // Opens \\.\PhysicalDriveN starting with the boot drive
        DWORD bootNum = GetBootDriveNumber();
        const DWORD kMaxDrives = 8;

        // Try the boot drive first, then the others
        DWORD order[kMaxDrives];
        DWORD count = 0;
        if (bootNum != DWORD(-1) && bootNum < kMaxDrives)
            order[count++] = bootNum;
        for (DWORD i = 0; i < kMaxDrives; i++)
            if (i != bootNum)
                order[count++] = i;

        for (DWORD di = 0; di < count; di++)
        {
            DWORD driveIdx = order[di];
            std::string path = "\\\\.\\PhysicalDrive" + std::to_string(driveIdx);

            // Try with and without elevated permission (as per idfinder)
            HANDLE h = CreateFileA(
                path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr
            );

            if (h == INVALID_HANDLE_VALUE)
            {
                h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            }
            if (h == INVALID_HANDLE_VALUE)
                continue;

            // Check if it's fixed media
            DISK_GEOMETRY dg{};
            DWORD bytes = 0;
            if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &dg, sizeof(dg), &bytes, nullptr) ||
                dg.MediaType != FixedMedia)
            {
                CloseHandle(h);
                continue;
            }

            // Get storage properties (includes serial)
            STORAGE_PROPERTY_QUERY spq{};
            spq.PropertyId = StorageDeviceProperty;
            spq.QueryType = PropertyStandardQuery;

            STORAGE_DESCRIPTOR_HEADER sdh{};
            if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), &sdh, sizeof(sdh), &bytes, nullptr) || sdh.Size == 0)
            {
                CloseHandle(h);
                continue;
            }

            std::vector<BYTE> buf(sdh.Size, 0);
            if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), buf.data(), sdh.Size, &bytes, nullptr))
            {
                CloseHandle(h);
                continue;
            }
            CloseHandle(h);

            auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buf.data());

            // Only SATA/PATA/NVMe — ignore USB, SD card etc.
            if (desc->BusType != BusTypeSata && desc->BusType != BusTypeAta && desc->BusType != BusTypeNvme && desc->BusType != BusTypeRAID)
                continue;

            if (!desc->SerialNumberOffset)
                continue;

            std::string serial(reinterpret_cast<const char*>(buf.data() + desc->SerialNumberOffset));

            // ATA stores serial with swapped bytes (idfinder: change_byte_order)
            if (desc->BusType == BusTypeSata || desc->BusType == BusTypeAta)
            {
                for (size_t i = 0; i + 1 < serial.size(); i += 2)
                    std::swap(serial[i], serial[i + 1]);
            }

            std::string s = Sanitize(serial);
            if (!s.empty())
                return s;
        }
        return {};
    }

    // Entry point for seed2 — tries the 3 paths in order
    std::string CollectBootDiskSerial()
    {
        std::string s;

        if (IsWindows8OrGreater())
        {
            s = CollectBootDiskSerial_MSFT(); // Path A — Win8+
            if (!s.empty())
                return s;
        }

        s = CollectBootDiskSerial_Win32(); // Path B — Vista/7 via WMI
        if (!s.empty())
            return s;

        s = CollectBootDiskSerial_IOCTL(); // Path C — without WMI
        return s;
    }

    // -----------------------------------------------------------------------
    // Fallback: Persistent seed in registry
    // -----------------------------------------------------------------------
    std::string GetOrCreateRegistryFallback()
    {
        const char* keyPath = "Software\\NextClient";
        const char* valueName = "HwidSeed";

        HKEY hKey = nullptr;
        char buf[64]{};
        DWORD bufSize = sizeof(buf);
        DWORD dwType = REG_SZ;

        if (RegCreateKeyExA(
                HKEY_CURRENT_USER, keyPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, nullptr, &hKey, nullptr
            ) != ERROR_SUCCESS)
            return "fallback_no_registry";

        LONG res = RegQueryValueExA(hKey, valueName, nullptr, &dwType, reinterpret_cast<LPBYTE>(buf), &bufSize);
        if (res == ERROR_SUCCESS && bufSize > 1)
        {
            RegCloseKey(hKey);
            return std::string(buf, bufSize - 1);
        }

        ULONGLONG tick = GetTickCount64();
        DWORD pid = GetCurrentProcessId();
        std::ostringstream oss;
        oss << std::hex << tick << pid;
        std::string seed = oss.str();

        RegSetValueExA(hKey, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(seed.c_str()), static_cast<DWORD>(seed.size() + 1));
        RegCloseKey(hKey);
        return seed;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
namespace hwid
{
    std::string Collect()
    {
        if (g_hwid_ready)
            return g_hwid_cache;

        std::string seed1 = CollectMotherboardUUID(); // Motherboard UUID
        std::string seed2 = CollectBootDiskSerial(); // Boot Disk Serial

        // Complete Fallback: no identifiable hardware
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

} // namespace hwid
