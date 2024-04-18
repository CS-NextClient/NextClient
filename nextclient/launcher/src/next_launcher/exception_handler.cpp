#include <ctime>
#include <string>
#include <Windows.h>
#include <dbghelp.h>
#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

bool g_SaveFullDumps;

static std::string FormatDumpFileName(const std::string& type)
{
    time_t rawtime;
    time(&rawtime);

    tm timeinfo{};

    localtime_s(&timeinfo, &rawtime);

    char formatTime[80];
    strftime(formatTime, sizeof(formatTime), "%d-%m-%Y-%H_%M_%S", &timeinfo);

    std::string dumpFilePath = type + "-" + std::string(formatTime) + ".mdmp";

    return dumpFilePath;
}

static void SaveFullDump(PEXCEPTION_POINTERS exception_pointers)
{
    std::string fileName = FormatDumpFileName("full_dump");

    HANDLE hFile = CreateFileA(fileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exception_pointers;
    exceptionInfo.ClientPointers = FALSE;

    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory | MiniDumpWithFullMemory | MiniDumpWithProcessThreadData | MiniDumpWithPrivateReadWriteMemory),
        exception_pointers ? &exceptionInfo : NULL,
        NULL,
        NULL);

    CloseHandle(hFile);
}

void ExceptionHandler(void* exception_pointers)
{
    if (g_SaveFullDumps)
        SaveFullDump((EXCEPTION_POINTERS*) exception_pointers);

#ifdef SENTRY_ENABLE
    sentry_ucontext_t ucontext;
    ucontext.exception_ptrs = *(EXCEPTION_POINTERS*) exception_pointers;
    sentry_handle_exception(&ucontext);
#endif
}
