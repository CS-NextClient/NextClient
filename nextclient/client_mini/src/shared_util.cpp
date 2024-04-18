#include <cstdarg>
#include <cstdio>
#include <strtools.h>

static char s_shared_token[1500];

char *SharedGetToken()
{
    return s_shared_token;
}

char *SharedParse(char *data)
{
    int len = 0, c;
    s_shared_token[0] = '\0';

    if (!data)
        return NULL;

    skipwhite:
    while ((c = *data) <= ' ')
    {
        if (c == 0)
            return NULL;

        data++;
    }

    if (c == '/' && data[1] == '/')
    {
        while (*data && *data != '\n')
            data++;

        goto skipwhite;
    }

    if (c == '\"')
    {
        data++;

        while (1)
        {
            c = *data++;

            if (c == '\"' || !c)
            {
                s_shared_token[len] = '\0';
                return data;
            }

            s_shared_token[len++] = c;
        }
    }

    if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ',')
    {
        s_shared_token[len++] = c;
        s_shared_token[len] = '\0';
        return data + 1;
    }

    do
    {
        s_shared_token[len] = c;
        data++;
        len++;
        c = *data;

        if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ',')
            break;
    }
    while (c > 32);

    s_shared_token[len] = '\0';
    return data;
}

const char *SharedParse(const char *data)
{
    int len = 0, c;
    s_shared_token[0] = '\0';

    if (!data)
        return NULL;

    skipwhite:
    while ((c = *data) <= ' ')
    {
        if (c == 0)
            return NULL;

        data++;
    }

    if (c == '/' && data[1] == '/')
    {
        while (*data && *data != '\n')
            data++;

        goto skipwhite;
    }

    if (c == '\"')
    {
        data++;

        while (1)
        {
            c = *data++;

            if (c == '\"' || !c)
            {
                s_shared_token[len] = '\0';
                return data;
            }

            s_shared_token[len++] = c;
        }
    }

    if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ',')
    {
        s_shared_token[len++] = c;
        s_shared_token[len] = '\0';
        return data + 1;
    }

    do
    {
        s_shared_token[len] = c;
        data++;
        len++;
        c = *data;

        if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ',')
            break;
    }
    while (c > 32);

    s_shared_token[len] = '\0';
    return data;
}

char *SharedVarArgs(char *format, ...)
{
    va_list argptr;
    const int BufLen = 1024;
    const int NumBuffers = 4;
    static char string[NumBuffers][BufLen];
    static int curstring = 0;

    curstring = (curstring + 1) % NumBuffers;

        va_start(argptr, format);
    _vsnprintf(string[curstring], BufLen, format, argptr);
        va_end(argptr);

    return string[curstring];
}