//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once

#if !USE_UWP && !USE_TIZEN
#include <exception>
#include <signal.h>

namespace gameanalytics
{
    namespace errorreporter
    {
        class GAUncaughtExceptionHandler
        {
        public:
            static void setUncaughtExceptionHandlers();
        private:
#if defined(_WIN32)
            static void signalHandler(int sig);
            static void (*old_state_ill) (int);
            static void (*old_state_abrt) (int);
            static void (*old_state_fpe) (int);
            static void (*old_state_segv) (int);
#else
            static void signalHandler(int sig, siginfo_t *info, void *context);
            static struct sigaction prevSigAction;
#endif
            static void formatConcat(char* buffer, const char* format, ...);
            static size_t formatSize(const char* format, ...);
            static void setupUncaughtSignals();
            static void terminateHandler();

            static std::terminate_handler previousTerminateHandler;
            static int errorCount;
            static int MAX_ERROR_TYPE_COUNT;
        };
    }
}
#endif
