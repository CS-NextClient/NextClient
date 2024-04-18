#pragma once

void ExceptionHandler(void* exception_pointers);

// Flag determines whether full memory dumps will be saved in addition to minidumps (such dumps will not be sent to Sentry)
extern bool g_SaveFullDumps;
