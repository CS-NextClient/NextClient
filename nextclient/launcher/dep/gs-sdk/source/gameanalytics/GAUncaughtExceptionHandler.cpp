//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//
#if !USE_UWP && !USE_TIZEN
#include "GAUncaughtExceptionHandler.h"
#include "GAState.h"
#include "GAEvents.h"
#include <stacktrace/call_stack.hpp>
#include <string.h>
#include <stdio.h>
#include <cstdarg>
#if defined(_WIN32)
#include <stdlib.h>
#include <tchar.h>
#else
#include <execinfo.h>
#include <cstring>
#include <cstdlib>
#endif

namespace gameanalytics
{
    namespace errorreporter
    {
        std::terminate_handler GAUncaughtExceptionHandler::previousTerminateHandler = NULL;
#if defined(_WIN32)
        void (*GAUncaughtExceptionHandler::old_state_ill) (int) = NULL;
        void (*GAUncaughtExceptionHandler::old_state_abrt) (int) = NULL;
        void (*GAUncaughtExceptionHandler::old_state_fpe) (int) = NULL;
        void (*GAUncaughtExceptionHandler::old_state_segv) (int) = NULL;
#else
        struct sigaction GAUncaughtExceptionHandler::prevSigAction;
#endif
        int GAUncaughtExceptionHandler::errorCount = 0;
        int GAUncaughtExceptionHandler::MAX_ERROR_TYPE_COUNT = 5;

#if defined(_WIN32)
        void GAUncaughtExceptionHandler::signalHandler(int sig)
        {
            if(state::GAState::useErrorReporting())
            {
                if(errorCount <= MAX_ERROR_TYPE_COUNT)
                {
                    stacktrace::call_stack st;
                    size_t totalSize = 0;
                    totalSize += formatSize("Uncaught Signal (%d)\n", sig);
                    totalSize += strlen("Stack trace:\n");
                    totalSize += st.to_string_size() + strlen("\n");
                    char* buffer = new char[totalSize + 1];
                    buffer[0] = 0;

                    formatConcat(buffer, "Uncaught Signal (%d)\n", sig);
                    strcat(buffer, "Stack trace:\n");
                    st.to_string(buffer);
                    strcat(buffer, "\n");

                    errorCount = errorCount + 1;
                    events::GAEvents::addErrorEvent(EGAErrorSeverity::Critical, buffer, {}, false);
                    events::GAEvents::processEvents("error", false);
                    delete[] buffer;
                }
            }

            if(sig == SIGILL && old_state_ill != NULL)
            {
                old_state_ill(sig);
            }
            else if(sig == SIGABRT && old_state_abrt != NULL)
            {
                old_state_abrt(sig);
            }
            else if(sig == SIGFPE && old_state_fpe != NULL)
            {
                old_state_fpe(sig);
            }
            else if(sig == SIGSEGV && old_state_segv != NULL)
            {
                old_state_segv(sig);
            }
        }

        void GAUncaughtExceptionHandler::setupUncaughtSignals()
        {
            signal(SIGILL, signalHandler);
            signal(SIGABRT, signalHandler);
            signal(SIGFPE, signalHandler);
            signal(SIGSEGV, signalHandler);
        }
#else
        void GAUncaughtExceptionHandler::setupUncaughtSignals()
        {
            struct sigaction mySigAction;
            mySigAction.sa_sigaction = signalHandler;
            mySigAction.sa_flags = SA_SIGINFO;

            sigemptyset(&mySigAction.sa_mask);
            sigaction(SIGQUIT, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGQUIT, &mySigAction, NULL);
            }
            sigaction(SIGILL, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGILL, &mySigAction, NULL);
            }
            sigaction(SIGTRAP, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGTRAP, &mySigAction, NULL);
            }
            sigaction(SIGABRT, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGABRT, &mySigAction, NULL);
            }
#if !USE_LINUX
            sigaction(SIGEMT, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGEMT, &mySigAction, NULL);
            }
#endif
            sigaction(SIGFPE, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGFPE, &mySigAction, NULL);
            }
            sigaction(SIGBUS, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGBUS, &mySigAction, NULL);
            }
            sigaction(SIGSEGV, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGSEGV, &mySigAction, NULL);
            }
            sigaction(SIGSYS, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGSYS, &mySigAction, NULL);
            }
            sigaction(SIGPIPE, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGPIPE, &mySigAction, NULL);
            }
            sigaction(SIGALRM, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGALRM, &mySigAction, NULL);
            }
            sigaction(SIGXCPU, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGXCPU, &mySigAction, NULL);
            }
            sigaction(SIGXFSZ, NULL, &prevSigAction);
            if (prevSigAction.sa_handler != SIG_IGN)
            {
                sigaction(SIGXFSZ, &mySigAction, NULL);
            }
        }

        /*    signalHandler
         *
         *        Set up the uncaught signals
         */
        void GAUncaughtExceptionHandler::signalHandler(int sig, siginfo_t *info, void *context)
        {
            if(state::GAState::useErrorReporting())
            {
                void *frames[128];
                int i,len = backtrace(frames, 128);
                char **symbols = backtrace_symbols(frames,len);

                /*
                 *    Now format into a message for sending to the user
                 */
                size_t totalSize = 0;
                /*totalSize += strlen("Uncaught Signal\n");
                totalSize += formatSize("si_signo    %d\n", info->si_signo);
                totalSize += formatSize("si_code     %d\n", info->si_code);
                totalSize += formatSize("si_value    %d\n", info->si_value);
                totalSize += formatSize("si_errno    %d\n", info->si_errno);
                totalSize += formatSize("si_addr     0x%08lX\n", info->si_addr);
                totalSize += formatSize("si_status   %d\n", info->si_status);*/
                totalSize += strlen("Stack trace:\n");
                for (i = 0; i < len; ++i)
                {
                    totalSize += formatSize("%4d - %s\n", i, symbols[i]);
                }

                char buffer[totalSize + 1];
                buffer[0] = 0;
                /*strcat(buffer, "Uncaught Signal\n");

                formatConcat(buffer, "si_signo    %d\n", info->si_signo);
                formatConcat(buffer, "si_code     %d\n", info->si_code);
                formatConcat(buffer, "si_value    %d\n", info->si_value);
                formatConcat(buffer, "si_errno    %d\n", info->si_errno);
                formatConcat(buffer, "si_addr     0x%08lX\n", info->si_addr);
                formatConcat(buffer, "si_status   %d\n", info->si_status);*/
                strcat(buffer, "Stack trace:\n");
                for (i = 0; i < len; ++i)
                {
                    formatConcat(buffer, "%4d - %s\n", i, symbols[i]);
                }

                if(errorCount <= MAX_ERROR_TYPE_COUNT)
                {
                    errorCount = errorCount + 1;
                    events::GAEvents::addErrorEvent(EGAErrorSeverity::Critical, buffer, {}, false, false);
                    events::GAEvents::processEvents("error", false);
                }

                struct sigaction newact;
                newact.sa_flags = 0;
                sigemptyset(&newact.sa_mask);
                newact.sa_handler= SIG_DFL;
            }

            if(*prevSigAction.sa_handler != NULL)
            {
                (*prevSigAction.sa_handler)(sig);
            }
        }
#endif
        void GAUncaughtExceptionHandler::formatConcat(char* buffer, const char* format, ...)
        {
            va_list args;
            va_start (args, format);
            size_t len = std::vsnprintf(NULL, 0, format, args);
            va_end (args);
            char* formatted = new char[len + 1];
            va_start (args, format);
            std::vsnprintf(formatted, len + 1, format, args);
            va_end (args);
            strcat(buffer, formatted);
            delete[] formatted;
        }

        size_t GAUncaughtExceptionHandler::formatSize(const char* format, ...)
        {
            va_list args;
            va_start (args, format);
            size_t len = std::vsnprintf(NULL, 0, format, args);
            va_end (args);
            return len;
        }

        /*    terminateHandler
         *
         *        C++ exception terminate handler
         */
        void GAUncaughtExceptionHandler::terminateHandler()
        {
            if(state::GAState::useErrorReporting())
            {
                /*
                 *    Now format into a message for sending to the user
                 */

                if(errorCount <= MAX_ERROR_TYPE_COUNT)
                {
                    stacktrace::call_stack st;
                    size_t totalSize = 0;
                    totalSize += strlen("Uncaught C++ Exception\n");
                    totalSize += strlen("Stack trace:\n");
                    totalSize += st.to_string_size() + strlen("\n");
                    char* buffer = new char[totalSize + 1];

                    strcat(buffer, "Uncaught C++ Exception\n");
                    strcat(buffer, "Stack trace:\n");
                    st.to_string(buffer);
                    strcat(buffer, "\n");

                    errorCount = errorCount + 1;
                    events::GAEvents::addErrorEvent(EGAErrorSeverity::Critical, buffer, {}, false, false);
                    events::GAEvents::processEvents("error", false);
                    delete[] buffer;
                }
            }

            if(previousTerminateHandler != NULL)
            {
                previousTerminateHandler();
            }
        }

        void GAUncaughtExceptionHandler::setUncaughtExceptionHandlers()
        {
            if(state::GAState::useErrorReporting())
            {
                setupUncaughtSignals();
                previousTerminateHandler = std::set_terminate(terminateHandler);
            }
        }
    }
}
#endif
