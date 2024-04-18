#include <steam/steam_api.h>
#include <steam/steam_gameserver.h>
#include <steam_api_proxy/next_steam_api_proxy.h>

#ifdef _WINDOWS
#include <Windows.h>
#include <nitro_utils/platform.h>
#else
#include <climits>
#include <iostream>

#include <signal.h>
#include <execinfo.h>

#include <nitroapi/NitroApiInterface.h>
#include <client_mini/client_mini.h>
#undef GetProcAddress
#include <nitro_utils/platform.h>
#endif

using namespace nitro_utils;

typedef bool (*Bool_NoParams_Func) ();
typedef bool (*Void_NoParams_Func) ();
typedef bool (*Void_CCallbackBase_int_Func) (class CCallbackBase* pCallback, int iCallback);
typedef bool (*Void_CCallbackBase_Func) (class CCallbackBase* pCallback);
typedef bool (*Void_CCallbackBase_SteamAPICall_t_Func) (class CCallbackBase* pCallback, SteamAPICall_t hAPICall);

typedef bool (*SteamGameServer_Init_Func) (uint32 unIP, uint16 usSteamPort, uint16 usGamePort, uint16 usQueryPort, EServerMode eServerMode, const char* pchVersionString);
typedef ISteamGameServer* (*SteamGameServer_Func) ();

typedef void (*SteamAPI_UseBreakpadCrashHandler_Func) (char const* pchVersion, char const* pchDate, char const* pchTime, bool bFullMemoryDumps, void* pvContext, PFNPreMinidumpCallback m_pfnPreMinidumpCallback);
typedef void (*SteamAPI_SetBreakpadAppID_Func) (uint32 unAppID);
typedef void (*SteamAPI_WriteMiniDump_Func) (uint32 uStructuredExceptionCode, void* pvExceptionInfo, uint32 uBuildID);
typedef void (*SteamAPI_SetMiniDumpComment_Func) (const char* pchMsg);

typedef ISteamUser* (*SteamUser_Func) ();
typedef ISteamFriends* (*SteamFriends_Func) ();
typedef ISteamUtils* (*SteamUtils_Func) ();
typedef ISteamMatchmaking* (*SteamMatchmaking_Func) ();
typedef ISteamUserStats* (*SteamUserStats_Func) ();
typedef ISteamApps* (*SteamApps_Func) ();
typedef ISteamNetworking* (*SteamNetworking_Func) ();
typedef ISteamMatchmakingServers* (*SteamMatchmakingServers_Func) ();
typedef ISteamRemoteStorage* (*SteamRemoteStorage_Func) ();
typedef ISteamScreenshots* (*SteamScreenshots_Func) ();
typedef ISteamHTTP* (*SteamHTTP_Func) ();
typedef ISteamUnifiedMessages* (*SteamUnifiedMessages_Func) ();

static SysModule g_ValueModule;

static NextSteamProxy_ExceptionFunc g_ExceptionCallback;

static Bool_NoParams_Func SteamAPI_Init_func = nullptr;
static Void_NoParams_Func SteamAPI_Shutdown_func = nullptr;
static Void_NoParams_Func SteamAPI_RunCallbacks_func = nullptr;
static Void_CCallbackBase_int_Func SteamAPI_RegisterCallback_func = nullptr;
static Void_CCallbackBase_Func SteamAPI_UnregisterCallback_func = nullptr;
static Void_CCallbackBase_SteamAPICall_t_Func SteamAPI_RegisterCallResult_func = nullptr;
static Void_CCallbackBase_SteamAPICall_t_Func SteamAPI_UnregisterCallResult_func = nullptr;
static SteamAPI_UseBreakpadCrashHandler_Func SteamAPI_UseBreakpadCrashHandler_func = nullptr;
static SteamAPI_SetBreakpadAppID_Func SteamAPI_SetBreakpadAppID_func = nullptr;
static SteamAPI_WriteMiniDump_Func SteamAPI_WriteMiniDump_func = nullptr;
static SteamAPI_SetMiniDumpComment_Func SteamAPI_SetMiniDumpComment_func = nullptr;
static SteamUser_Func SteamUser_func = nullptr;
static SteamFriends_Func SteamFriends_func = nullptr;
static SteamUtils_Func SteamUtils_func = nullptr;
static SteamMatchmaking_Func SteamMatchmaking_func = nullptr;
static SteamUserStats_Func SteamUserStats_func = nullptr;
static SteamApps_Func SteamApps_func = nullptr;
static SteamNetworking_Func SteamNetworking_func = nullptr;
static SteamMatchmakingServers_Func SteamMatchmakingServers_func = nullptr;
static SteamRemoteStorage_Func SteamRemoteStorage_func = nullptr;
static SteamScreenshots_Func SteamScreenshots_func = nullptr;
static SteamHTTP_Func SteamHTTP_func = nullptr;
static SteamUnifiedMessages_Func SteamUnifiedMessages_func = nullptr;
static SteamGameServer_Init_Func SteamGameServer_Init_func = nullptr;
static Void_NoParams_Func SteamGameServer_Shutdown_func = nullptr;
static Void_NoParams_Func SteamGameServer_RunCallbacks_func = nullptr;
static SteamGameServer_Func SteamGameServer_func = nullptr;

#ifdef _WINDOWS
void Initialize()
{
    g_ValueModule = LoadSysModule("steam_api_orig.dll");
    if (g_ValueModule != nullptr)
        return;

    g_ValueModule = LoadSysModule("platform\\steam\\steam_api_orig.dll");
    if (g_ValueModule != nullptr)
        return;

    MessageBoxA(nullptr, "steam_api_orig.dll not found", "", MB_OK | MB_ICONERROR);
}
#else
void SigHandler(int signum)
{
    std::cerr << "Crash! signal " << signum << "\n";
    std::cerr << "See backtrace in crash_backtrace.txt\n";

    void* array[100];
    size_t size = backtrace(array, sizeof(array));
    char** strings = backtrace_symbols(array, size);

    std::ofstream outfile("crash_backtrace.txt", std::ios_base::app);
    if (outfile.is_open())
    {
        outfile << "Signal " << signum << "\n";
        if (strings != nullptr)
        {
            for (size_t i = 0; i < size; i++) {
                outfile << strings[i] << "\n";
            }
        }
        outfile.close();
    }

    fflush(stdout);
    fflush(stderr);

    signal(signum, SIG_DFL);
    exit(3);
}

void SetSigHandlers()
{
    signal(SIGSEGV, SigHandler);
    signal(SIGFPE, SigHandler);
    signal(SIGILL, SigHandler);
    signal(SIGABRT, SigHandler);
}

void Initialize()
{
    remove("crash_backtrace.txt");
    SetSigHandlers();

    g_ValueModule = LoadSysModule("libsteam_api_orig.so");
    if (g_ValueModule == nullptr)
        std::cerr << "g_ValueModule is null\n";
    CSysModule* nitro_module = Sys_LoadModule("nitro_api2.so");
    CreateInterfaceFn nitro_factory = Sys_GetFactory(nitro_module);
    if (nitro_factory == nullptr)
        std::cerr << "nitro_factory is null\n";
    nitroapi::NitroApiInterface* nitroapi = (nitroapi::NitroApiInterface*)nitro_factory(NITROAPI_INTERFACE_VERSION, nullptr);

    CSysModule* client_mini_module = Sys_LoadModule("cstrike/cl_dlls/client_mini.so");
    CreateInterfaceFn client_mini_factory = Sys_GetFactory(client_mini_module);
    if (client_mini_factory == nullptr)
        std::cerr << "client_mini_factory is null\n";
    ClientMiniInterface* client_mini = (ClientMiniInterface*)client_mini_factory(CLIENT_MINI_INTERFACE_VERSION, nullptr);

    nitroapi->Initialize(nullptr, nullptr, nullptr);
    client_mini->Init(nitroapi);
}
#endif

void UnInitialize()
{
    if (g_ValueModule == nullptr)
        return;

    UnloadSysModule(g_ValueModule);
    g_ValueModule = nullptr;

    g_ExceptionCallback = nullptr;

    SteamAPI_Init_func = nullptr;
    SteamAPI_Shutdown_func = nullptr;
    SteamAPI_RunCallbacks_func = nullptr;
    SteamAPI_RegisterCallback_func = nullptr;
    SteamAPI_UnregisterCallback_func = nullptr;
    SteamAPI_RegisterCallResult_func = nullptr;
    SteamAPI_UnregisterCallResult_func = nullptr;
    SteamAPI_UseBreakpadCrashHandler_func = nullptr;
    SteamAPI_SetBreakpadAppID_func = nullptr;
    SteamAPI_WriteMiniDump_func = nullptr;
    SteamAPI_SetMiniDumpComment_func = nullptr;
    SteamUser_func = nullptr;
    SteamFriends_func = nullptr;
    SteamUtils_func = nullptr;
    SteamMatchmaking_func = nullptr;
    SteamUserStats_func = nullptr;
    SteamApps_func = nullptr;
    SteamNetworking_func = nullptr;
    SteamMatchmakingServers_func = nullptr;
    SteamRemoteStorage_func = nullptr;
    SteamScreenshots_func = nullptr;
    SteamHTTP_func = nullptr;
    SteamUnifiedMessages_func = nullptr;
    SteamGameServer_Init_func = nullptr;
    SteamGameServer_Shutdown_func = nullptr;
    SteamGameServer_RunCallbacks_func = nullptr;
    SteamGameServer_func = nullptr;
}

bool IsInitialized() { return g_ValueModule != nullptr; }

S_API void NextSteamProxy_SetSEH(NextSteamProxy_ExceptionFunc exception_callback)
{
    g_ExceptionCallback = exception_callback;
}

S_API bool SteamAPI_Init()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_Init_func)
        SteamAPI_Init_func = reinterpret_cast<Bool_NoParams_Func>(GetProcAddress(g_ValueModule, "SteamAPI_Init"));

    return SteamAPI_Init_func();
}

S_API void SteamAPI_Shutdown()
{
    if (IsInitialized())
    {
        if (!SteamAPI_Shutdown_func)
            SteamAPI_Shutdown_func = reinterpret_cast<Void_NoParams_Func>(GetProcAddress(g_ValueModule, "SteamAPI_Shutdown"));

        SteamAPI_Shutdown_func();
    }

    UnInitialize();
}

S_API void SteamAPI_RunCallbacks()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_RunCallbacks_func)
        SteamAPI_RunCallbacks_func = reinterpret_cast<Void_NoParams_Func>(GetProcAddress(g_ValueModule, "SteamAPI_RunCallbacks"));

    SteamAPI_RunCallbacks_func();
}

S_API void SteamAPI_RegisterCallback(class CCallbackBase* pCallback, int iCallback)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_RegisterCallback_func)
        SteamAPI_RegisterCallback_func = reinterpret_cast<Void_CCallbackBase_int_Func>(GetProcAddress(g_ValueModule, "SteamAPI_RegisterCallback"));

    SteamAPI_RegisterCallback_func(pCallback, iCallback);
}

S_API void SteamAPI_UnregisterCallback(class CCallbackBase* pCallback)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_UnregisterCallback_func)
        SteamAPI_UnregisterCallback_func = reinterpret_cast<Void_CCallbackBase_Func>(GetProcAddress(g_ValueModule, "SteamAPI_UnregisterCallback"));

    SteamAPI_UnregisterCallback_func(pCallback);
}

S_API void SteamAPI_RegisterCallResult(class CCallbackBase* pCallback, SteamAPICall_t hAPICall)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_RegisterCallResult_func)
        SteamAPI_RegisterCallResult_func = reinterpret_cast<Void_CCallbackBase_SteamAPICall_t_Func>(GetProcAddress(g_ValueModule, "SteamAPI_RegisterCallResult"));

    SteamAPI_RegisterCallResult_func(pCallback, hAPICall);
}

S_API void SteamAPI_UnregisterCallResult(class CCallbackBase* pCallback, SteamAPICall_t hAPICall)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_UnregisterCallResult_func)
        SteamAPI_UnregisterCallResult_func = reinterpret_cast<Void_CCallbackBase_SteamAPICall_t_Func>(GetProcAddress(g_ValueModule, "SteamAPI_UnregisterCallResult"));

    SteamAPI_UnregisterCallResult_func(pCallback, hAPICall);
}

S_API void SteamAPI_UseBreakpadCrashHandler(char const* pchVersion, char const* pchDate, char const* pchTime, bool bFullMemoryDumps, void* pvContext, PFNPreMinidumpCallback m_pfnPreMinidumpCallback)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_UseBreakpadCrashHandler_func)
        SteamAPI_UseBreakpadCrashHandler_func = reinterpret_cast<SteamAPI_UseBreakpadCrashHandler_Func>(GetProcAddress(g_ValueModule, "SteamAPI_UseBreakpadCrashHandler"));

    SteamAPI_UseBreakpadCrashHandler_func(pchVersion, pchDate, pchTime, bFullMemoryDumps, pvContext, m_pfnPreMinidumpCallback);
}

S_API void SteamAPI_SetBreakpadAppID(uint32 unAppID)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_SetBreakpadAppID_func)
        SteamAPI_SetBreakpadAppID_func = reinterpret_cast<SteamAPI_SetBreakpadAppID_Func>(GetProcAddress(g_ValueModule, "SteamAPI_SetBreakpadAppID"));

    SteamAPI_SetBreakpadAppID_func(unAppID);

#ifndef _WINDOWS
    SetSigHandlers();
#endif
}

S_API void SteamAPI_WriteMiniDump(uint32 uStructuredExceptionCode, void* pvExceptionInfo, uint32 uBuildID)
{
    if (!IsInitialized())
        Initialize();

    if (g_ExceptionCallback != nullptr)
        g_ExceptionCallback(pvExceptionInfo);

    if (!SteamAPI_WriteMiniDump_func)
        SteamAPI_WriteMiniDump_func = reinterpret_cast<SteamAPI_WriteMiniDump_Func>(GetProcAddress(g_ValueModule, "SteamAPI_WriteMiniDump"));

    SteamAPI_WriteMiniDump_func(uStructuredExceptionCode, pvExceptionInfo, uBuildID);
}

S_API void SteamAPI_SetMiniDumpComment(const char* pchMsg)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamAPI_SetMiniDumpComment_func)
        SteamAPI_SetMiniDumpComment_func = reinterpret_cast<SteamAPI_SetMiniDumpComment_Func>(GetProcAddress(g_ValueModule, "SteamAPI_SetMiniDumpComment"));

    SteamAPI_SetMiniDumpComment_func(pchMsg);
}

S_API ISteamUser *SteamUser()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamUser_func)
        SteamUser_func = reinterpret_cast<SteamUser_Func>(GetProcAddress(g_ValueModule, "SteamUser"));

    return SteamUser_func();
}

S_API ISteamFriends *SteamFriends()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamFriends_func)
        SteamFriends_func = reinterpret_cast<SteamFriends_Func>(GetProcAddress(g_ValueModule, "SteamFriends"));

    return SteamFriends_func();
}

S_API ISteamUtils *SteamUtils()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamUtils_func)
        SteamUtils_func = reinterpret_cast<SteamUtils_Func>(GetProcAddress(g_ValueModule, "SteamUtils"));

    return SteamUtils_func();
}

S_API ISteamMatchmaking *SteamMatchmaking()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamMatchmaking_func)
        SteamMatchmaking_func = reinterpret_cast<SteamMatchmaking_Func>(GetProcAddress(g_ValueModule, "SteamMatchmaking"));

    return SteamMatchmaking_func();
}

S_API ISteamUserStats *SteamUserStats()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamUserStats_func)
        SteamUserStats_func = reinterpret_cast<SteamUserStats_Func>(GetProcAddress(g_ValueModule, "SteamUserStats"));

    return SteamUserStats_func();
}

S_API ISteamApps *SteamApps()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamApps_func)
        SteamApps_func = reinterpret_cast<SteamApps_Func>(GetProcAddress(g_ValueModule, "SteamApps"));

    return SteamApps_func();
}

S_API ISteamNetworking *SteamNetworking()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamNetworking_func)
        SteamNetworking_func = reinterpret_cast<SteamNetworking_Func>(GetProcAddress(g_ValueModule, "SteamNetworking"));

    return SteamNetworking_func();
}

S_API ISteamMatchmakingServers *SteamMatchmakingServers()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamMatchmakingServers_func)
        SteamMatchmakingServers_func = reinterpret_cast<SteamMatchmakingServers_Func>(GetProcAddress(g_ValueModule, "SteamMatchmakingServers"));

    return SteamMatchmakingServers_func();
}

S_API ISteamRemoteStorage *SteamRemoteStorage()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamRemoteStorage_func)
        SteamRemoteStorage_func = reinterpret_cast<SteamRemoteStorage_Func>(GetProcAddress(g_ValueModule, "SteamRemoteStorage"));

    return SteamRemoteStorage_func();
}

S_API ISteamScreenshots *SteamScreenshots()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamScreenshots_func)
        SteamScreenshots_func = reinterpret_cast<SteamScreenshots_Func>(GetProcAddress(g_ValueModule, "SteamScreenshots"));

    return SteamScreenshots_func();
}

S_API ISteamHTTP *SteamHTTP()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamHTTP_func)
        SteamHTTP_func = reinterpret_cast<SteamHTTP_Func>(GetProcAddress(g_ValueModule, "SteamHTTP"));

    return SteamHTTP_func();
}

S_API ISteamUnifiedMessages *SteamUnifiedMessages()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamUnifiedMessages_func)
        SteamUnifiedMessages_func = reinterpret_cast<SteamUnifiedMessages_Func>(GetProcAddress(g_ValueModule, "SteamUnifiedMessages"));

    return SteamUnifiedMessages_func();
}

S_API bool SteamGameServer_Init(uint32 unIP, uint16 usSteamPort, uint16 usGamePort, uint16 usQueryPort, EServerMode eServerMode, const char* pchVersionString)
{
    if (!IsInitialized())
        Initialize();

    if (!SteamGameServer_Init_func)
        SteamGameServer_Init_func = reinterpret_cast<SteamGameServer_Init_Func>(GetProcAddress(g_ValueModule, "SteamGameServer_Init"));

    return SteamGameServer_Init_func(unIP, usSteamPort, usGamePort, usQueryPort, eServerMode, pchVersionString);
}

S_API void SteamGameServer_Shutdown()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamGameServer_Shutdown_func)
        SteamGameServer_Shutdown_func = reinterpret_cast<Void_NoParams_Func>(GetProcAddress(g_ValueModule, "SteamGameServer_Shutdown"));

    SteamGameServer_Shutdown_func();
}

S_API void SteamGameServer_RunCallbacks()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamGameServer_RunCallbacks_func)
        SteamGameServer_RunCallbacks_func = reinterpret_cast<Void_NoParams_Func>(GetProcAddress(g_ValueModule, "SteamGameServer_RunCallbacks"));

    SteamGameServer_RunCallbacks_func();
}

S_API ISteamGameServer* SteamGameServer()
{
    if (!IsInitialized())
        Initialize();

    if (!SteamGameServer_func)
        SteamGameServer_func = reinterpret_cast<SteamGameServer_Func>(GetProcAddress(g_ValueModule, "SteamGameServer"));

    return SteamGameServer_func();
}
