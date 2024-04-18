#pragma once

#include <cstring>

char *SharedGetToken();
char *SharedParse(char *data);
const char *SharedParse(const char *data);
char *SharedVarArgs(char *format, ...);

inline char *CloneString(const char *str)
{
    char *cloneStr = new char [strlen(str) + 1];
    strcpy(cloneStr, str);
    return cloneStr;
}

inline wchar_t *CloneWString(const wchar_t *str)
{
    wchar_t *cloneStr = new wchar_t [wcslen(str) + 1];
    wcscpy(cloneStr, str);
    return cloneStr;
}