#pragma once
#include "../engine.h"

enum class ConLogType
{
    Info,
    InfoColored,
};

inline std::array<Color, sizeof(ConLogType)> g_LogTypeColors {
    Color{},                      // Info
    Color{173, 200, 247},   // InfoColored
};

extern bool con_debuglog;

void Con_Init();
template<class... TArgs>
char* Con_Printf(const char* format, TArgs&&... args) { return eng()->Con_Printf.GetFunc()(format, std::forward<TArgs>(args)...); }
void Con_DPrintf(ConLogType type, const char* format, ...);
