#pragma once
#include "../engine.h"

enum class ConLogType
{
    Info,
    InfoColored,
    Warning,
    Error
};

inline std::array<Color, sizeof(ConLogType)> g_LogTypeColors {
    Color{},                // Info
    Color{173, 200, 247},   // InfoColored
    Color{229, 153, 0},     // Warning
    Color{242, 43, 43},     // Error
};

extern bool con_debuglog;
extern bool con_initialized;

void Con_Init();
template<class... TArgs>
char* Con_Printf(const char* format, TArgs&&... args) { return eng()->Con_Printf.GetFunc()(format, std::forward<TArgs>(args)...); }
void Con_DPrintf(ConLogType type, const char* format, ...);
