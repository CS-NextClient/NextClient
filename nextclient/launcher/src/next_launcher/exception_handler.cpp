#include <atomic>
#include <cstdlib>

#ifdef _WIN32
    #include <Windows.h>
    #include <dbghelp.h>
#else
    #include <cstdio>
    #include <ctime>
    #include <execinfo.h>
#endif
#ifdef SENTRY_ENABLE
#include <sentry.h>
#endif

#include "exception_handler.h"

bool g_SaveFullDumps;

namespace
{
#ifdef _WIN32
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
#else
    // No SEH/minidumps outside Windows - write a plain backtrace instead, same
    // pattern steam_api_proxy's SigHandler already uses. There's no "full memory"
    // equivalent for a backtrace, so g_SaveFullDumps only changes the file name.
    void BuildDumpPath(char* out, size_t out_size, const char* prefix)
    {
        time_t now = time(nullptr);
        tm local_time{};
        localtime_r(&now, &local_time);

        snprintf(out, out_size, "%s-%02d-%02d-%04d-%02d_%02d_%02d.txt", prefix,
            local_time.tm_mday, local_time.tm_mon + 1, local_time.tm_year + 1900,
            local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
    }

    void WriteBacktrace(const char* file_path)
    {
        FILE* file = fopen(file_path, "w");
        if (file == nullptr)
        {
            fprintf(stderr, "[exception_handler] backtrace write FAILED: %s\n", file_path);
            return;
        }

        void* frames[100];
        int frame_count = backtrace(frames, 100);
        backtrace_symbols_fd(frames, frame_count, fileno(file));

        fclose(file);

        fprintf(stderr, "[exception_handler] backtrace written: %s\n", file_path);
    }

    void SaveCrashDump(void*)
    {
        char path[512];
        BuildDumpPath(path, sizeof(path), g_SaveFullDumps ? "full_dump" : "minidump");
        WriteBacktrace(path);
    }
#endif
} // namespace

void ExceptionHandler(void* exception_pointers)
{
#ifdef _WIN32
    EXCEPTION_POINTERS* ep = (EXCEPTION_POINTERS*)exception_pointers;
    if (ep == nullptr)
    {
        return;
    }
#endif

    // Capture once per process. An outer engine/Steam filter can swallow a fatal fault and
    // resume on a deterministically failing operation, re-entering this handler every iteration;
    // the faulting address is unreliable for dedup (may be null or vary), so guard with a flag.
    static std::atomic<bool> handled = false;
    bool expected = false;
    if (!handled.compare_exchange_strong(expected, true))
    {
        return;
    }

    // Order is deliberate: capture locally and report to Sentry before terminating.
    // sentry_handle_exception captures synchronously (may not even return on the crashpad
    // backend), so the terminate below cannot truncate the report.
#ifdef _WIN32
    SaveCrashDump(ep);
#else
    SaveCrashDump(exception_pointers);
#endif

#ifdef SENTRY_ENABLE
    sentry_ucontext_t ucontext;
#ifdef _WIN32
    ucontext.exception_ptrs = *ep;
#endif
    sentry_handle_exception(&ucontext);
#endif

#ifdef _WIN32
    // Terminate so the swallow-and-continue loop can't re-enter the same fault and keep
    // dumping. This deliberately preempts Steam's crash handler, which NextClient doesn't use.
    TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : EXCEPTION_NONCONTINUABLE_EXCEPTION);
#else
    exit(1);
#endif
}
