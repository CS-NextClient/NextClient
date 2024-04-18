#include <cstdarg>
#include <strtools.h>

char* va(const char* format, ...)
{
    va_list argptr;
    static char string[256][1024], * s;
    static int stringindex = 0;

    s = string[stringindex];
    stringindex = (stringindex + 1) & 255;
    va_start(argptr, format);
    Q_vsnprintf(s, sizeof(string[0]), format, argptr);
    va_end(argptr);

    return s;
}
