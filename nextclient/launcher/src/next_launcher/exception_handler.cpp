#include <Windows.h>
#include <dbghelp.h>
#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

#include "exception_handler.h"

bool g_SaveFullDumps;

namespace
{
    // clang-format off
    constexpr MINIDUMP_TYPE kCompactDumpType = MINIDUMP_TYPE(
        MiniDumpWithDataSegs |
        MiniDumpWithProcessThreadData |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules |
        MiniDumpWithIndirectlyReferencedMemory);

    constexpr MINIDUMP_TYPE kFullDumpType = MINIDUMP_TYPE(
        MiniDumpWithFullMemory |
        MiniDumpWithFullMemoryInfo |
        MiniDumpWithHandleData |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules |
        MiniDumpWithProcessThreadData);
    // clang-format on

    void ReportDumpResult(const char* file_path, bool ok, DWORD last_error)
    {
        char line[512];
        wsprintfA(line, "[exception_handler] minidump %s: %s (error=%lu)\n", ok ? "written" : "FAILED", file_path, last_error);

        OutputDebugStringA(line);
    }

    void BuildDumpPath(char* out, const char* prefix)
    {
        SYSTEMTIME t;
        GetLocalTime(&t);

        wsprintfA(out, "%s-%02d-%02d-%04d-%02d_%02d_%02d.mdmp", prefix, t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute, t.wSecond);
    }

    bool WriteMiniDump(EXCEPTION_POINTERS* exception_pointers, MINIDUMP_TYPE dump_type, const char* file_path)
    {
        HANDLE file = CreateFileA(file_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            ReportDumpResult(file_path, false, GetLastError());
            return false;
        }

        MINIDUMP_EXCEPTION_INFORMATION exception_info;
        exception_info.ThreadId = GetCurrentThreadId();
        exception_info.ExceptionPointers = exception_pointers;
        exception_info.ClientPointers = FALSE;

        BOOL written = FALSE;
        __try
        {
            written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, dump_type, &exception_info, nullptr, nullptr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            written = FALSE;
        }

        DWORD last_error = written ? ERROR_SUCCESS : GetLastError();

        // Flush before Steam's crash handler fastfails the process out from under us.
        FlushFileBuffers(file);

        LARGE_INTEGER size{};
        GetFileSizeEx(file, &size);
        CloseHandle(file);

        if (!written || size.QuadPart == 0)
        {
            DeleteFileA(file_path);
            ReportDumpResult(file_path, false, last_error);
            return false;
        }

        ReportDumpResult(file_path, true, last_error);
        return true;
    }

    void SaveCrashDump(EXCEPTION_POINTERS* exception_pointers)
    {
        char path[MAX_PATH];

#ifdef SENTRY_ENABLE
        constexpr bool sentry_enabled = true;
#else
        constexpr bool sentry_enabled = false;
#endif

        if (!g_SaveFullDumps && !sentry_enabled)
        {
            BuildDumpPath(path, "minidump");
            WriteMiniDump(exception_pointers, kCompactDumpType, path);
        }

        if (g_SaveFullDumps)
        {
            BuildDumpPath(path, "full_dump");
            WriteMiniDump(exception_pointers, kFullDumpType, path);
        }
    }
} // namespace

void ExceptionHandler(void* exception_pointers)
{
    EXCEPTION_POINTERS* ep = (EXCEPTION_POINTERS*)exception_pointers;
    if (ep == nullptr)
    {
        return;
    }

    // Capture once per process. An outer engine/Steam filter can swallow a fatal fault and
    // resume on a deterministically failing operation, re-entering this handler every iteration;
    // the faulting address is unreliable for dedup (may be null or vary), so guard with a flag.
    static LONG handled = 0;
    if (InterlockedExchange(&handled, 1) != 0)
    {
        return;
    }

    // Order is deliberate: capture locally and report to Sentry before terminating.
    // sentry_handle_exception captures synchronously (may not even return on the crashpad
    // backend), so the terminate below cannot truncate the report.
    SaveCrashDump(ep);

#ifdef SENTRY_ENABLE
    sentry_ucontext_t ucontext;
    ucontext.exception_ptrs = *ep;
    sentry_handle_exception(&ucontext);
#endif

    // Terminate so the swallow-and-continue loop can't re-enter the same fault and keep
    // dumping. This deliberately preempts Steam's crash handler, which NextClient doesn't use.
    TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : EXCEPTION_NONCONTINUABLE_EXCEPTION);
}
