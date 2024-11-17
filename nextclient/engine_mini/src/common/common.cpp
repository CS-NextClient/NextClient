#include "../engine.h"
#include <common.h>
#include <optick.h>
#include "../console/console.h"
#include "../common/filesystem.h"
#include "../common/sys_dll.h"
#include "../common/zone.h"

int com_argc;
char** com_argv;
char com_cmdline[COM_MAX_CMD_LINE];

unsigned char COM_Nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return (unsigned char) (c - '0');
    }

    if (c >= 'A' && c <= 'F')
    {
        return (unsigned char) (c - 'A' + 0x0A);
    }

    if (c >= 'a' && c <= 'f')
    {
        return (unsigned char) (c - 'a' + 0x0A);
    }

    return '0';
}

void COM_HexConvert(const char* pszInput, int nInputLength, unsigned char* pOutput)
{
    const char* pIn;
    unsigned char* p = pOutput;
    for (int i = 0; i < nInputLength - 1; i += 2)
    {
        pIn = &pszInput[i];
        if (!pIn[0] || !pIn[1])
            break;

        *p++ = ((COM_Nibble(pIn[0]) << 4) | COM_Nibble(pIn[1]));
    }
}

void COM_ClearCustomizationList(customization_t* pHead, qboolean bCleanDecals)
{
    eng()->COM_ClearCustomizationList.InvokeChained(pHead, bCleanDecals);
}

int COM_SizeofResourceList(resource_t* pList, resourceinfo_t* ri)
{
    OPTICK_EVENT();

    int nSize = 0;
    resource_t* p;

    Q_memset(ri, 0, sizeof(*ri));

    for (p = pList->pNext; p != pList; p = p->pNext)
    {
        nSize += p->nDownloadSize;

        if (p->type == t_model && p->nIndex == 1)
            ri->info[t_world].size += p->nDownloadSize;
        else ri->info[p->type].size += p->nDownloadSize;
    }

    return nSize;
}

char* COM_BinPrintf(unsigned char* buf, int length)
{
    OPTICK_EVENT();

    char szChunk[10];
    static char szReturn[4096];
    Q_memset(szReturn, 0, sizeof(szReturn));

    for (int i = 0; i < length; i++)
    {
        Q_snprintf(szChunk, sizeof(szChunk), "%02x", buf[i]);
        Q_strcat(szReturn, szChunk, sizeof(szReturn));
    }

    return szReturn;
}

void COM_ExplainDisconnection(qboolean bPrint, const char *fmt, ...)
{
    OPTICK_EVENT();

    static char buffer[1024];

    va_list varargs;

    va_start(varargs, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, varargs);
    va_end(varargs);

    strncpy(gszDisconnectReason, buffer, kDisconnectReasonSize);
    gszDisconnectReason[kDisconnectReasonSize - 1] = 0;
    *gfExtendedError = 1;

    if (bPrint)
    {
        if (gszDisconnectReason[0] != '#')
            Con_Printf("%s\n", gszDisconnectReason);
    }
}

int COM_CheckString(const char* string)
{
    if (string && *string)
        return 1;

    return 0;
}

// notice this function works only if we are running as dedicated server
int COM_CheckParm(const char *parm)
{
    OPTICK_EVENT();

    int i;

    for (i = 1; i < com_argc; i++)
    {
        if (!com_argv[i])
        {
            continue;
        }

        if (!Q_strcmp(parm, (const char *) com_argv[i]))
        {
            return i;
        }
    }

    return 0;
}

void COM_InitArgv(int argc, char *argv[])
{
    OPTICK_EVENT();

    qboolean safe = 0;

    static const char *safeargvs[NUM_SAFE_ARGVS] =
    {
        "-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse", "-dibonly"
    };
    static const char *largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];

    int i, j;
    char *c;

    // Reconstruct full command line
    com_cmdline[0] = 0;
    for (i = 0, j = 0; i < MAX_NUM_ARGVS && i < argc && j < COM_MAX_CMD_LINE - 1; i++)
    {
        c = argv[i];
        if (*c)
        {
            while (*c && j < COM_MAX_CMD_LINE - 1)
            {
                com_cmdline[j++] = *c++;
            }
            if (j >= COM_MAX_CMD_LINE - 1)
            {
                break;
            }
            com_cmdline[j++] = ' ';
        }
    }
    com_cmdline[j] = 0;

    // Copy args pointers to our array
    for (com_argc = 0; (com_argc < MAX_NUM_ARGVS) && (com_argc < argc); com_argc++)
    {
        largv[com_argc] = argv[com_argc];

        if (!Q_strcmp("-safe", argv[com_argc]))
        {
            safe = 1;
        }
    }

    // Add arguments introducing more failsafeness
    if (safe)
    {
        // force all the safe-mode switches. Note that we reserved extra space in
        // case we need to add these, so we don't need an overflow check
        for (int i = 0; i < NUM_SAFE_ARGVS; i++)
        {
            largv[com_argc] = safeargvs[i];
            com_argc++;
        }
    }

    largv[com_argc] = " ";
    com_argv = const_cast<char**>(largv);
}

const char *COM_FileBase_s(const char *in, char *out, int size)
{
    OPTICK_EVENT();

    if (!in || !in[0])
    {
        *out = '\0';
        return NULL;
    }

    int len = Q_strlen(in);
    if (len <= 0)
        return NULL;

    // scan backward for '.'
    int end = len - 1;
    while (end && in[end] != '.' && !PATHSEPARATOR(in[end]))
        end--;

    // no '.', copy to end
    if (in[end] != '.')
    {
        end = len - 1;
    }
    else
    {
        // Found ',', copy to left of '.'
        end--;
    }

    // Scan backward for '/'
    int start = len - 1;
    while (start >= 0 && !PATHSEPARATOR(in[start]))
        start--;

    if (start < 0 || !PATHSEPARATOR(in[start]))
    {
        start = 0;
    }
    else
    {
        start++;
    }

    // Length of new sting
    int maxcopy = end - start + 1;
    if (size >= 0 && maxcopy >= size)
        return NULL;

    // Copy partial string
    std::strncpy(out, &in[start], maxcopy);
    out[maxcopy] = '\0';
    return out;
}

void COM_FileBase(const char *in, char *out)
{
    OPTICK_EVENT();

    COM_FileBase_s(in, out, -1);
}

unsigned char *COM_LoadStackFile(const char *path, void *buffer, int bufsize, int *length)
{
    OPTICK_EVENT();

    *p_loadbuf = (byte*)buffer;
    *p_loadsize = bufsize;

    return COM_LoadFile(path, 4, length);
}

unsigned char* COM_LoadFile(const char *path, int usehunk, int *pLength)
{
    OPTICK_EVENT();

    if (!path || !path[0])
        return nullptr;

    char base[MAX_PATH];
    unsigned char *buf = nullptr;

#ifndef SWDS
    p_g_engdstAddrs->COM_LoadFile(&path, &usehunk, &pLength);
#endif

    if (pLength)
        *pLength = 0;

    FileHandle_t hFile = FS_Open(path, "rb");

    if (!hFile)
        return nullptr;

    int len = FS_Size(hFile);

    if (!COM_FileBase_s(path, base, sizeof(base)))
        Sys_Error("%s: Bad path length: %s", __func__, path);

    base[32] = '\0';

    if (usehunk == 0)
        buf = (unsigned char *)Z_Malloc(len + 1);
    else if (usehunk == 1)
        buf = (unsigned char *)Hunk_AllocName(len + 1, base);
    else if (usehunk == 2)
        buf = (unsigned char *)Hunk_TempAlloc(len + 1);
    else if (usehunk == 3)
        buf = (unsigned char *)Cache_Alloc(*p_loadcache, len + 1, base);
    else if (usehunk == 4)
    {
        if (len + 1 <= *p_loadsize)
            buf = *p_loadbuf;
        else
            buf = (unsigned char *)Hunk_TempAlloc(len + 1);
    }
    else if (usehunk == 5)
        buf = (unsigned char *)Mem_Malloc(len + 1);
    else
        Sys_Error("COM_LoadFile: bad usehunk");

    if (!buf)
        Sys_Error("COM_LoadFile: not enough space for %s", path);

    buf[len] = 0;

    FS_Read(buf, len, hFile);
    FS_Close(hFile);

    if (pLength)
        *pLength = len;

    return buf;
}

unsigned char* COM_LoadFileForMe(const char* filename, int* pLength)
{
    OPTICK_EVENT();

    return COM_LoadFile(filename, 5, pLength);
}

unsigned char* COM_LoadFileLimit(const char* path, int pos, int cbmax, int* pcbread, FileHandle_t* phFile)
{
    OPTICK_EVENT();

    FileHandle_t hFile;
    unsigned char* buf;
    char base[32];
    int len;
    int cbload;

    hFile = *phFile;
    if (!hFile)
    {
        hFile = FS_Open(path, "rb");
        if (!hFile)
            return NULL;
    }

    len = FS_Size(hFile);
    if (len < pos)
        FS_Close(hFile);

    FS_Seek(hFile, pos, FILESYSTEM_SEEK_HEAD);

    if (len > cbmax)
        cbload = cbmax;
    else
        cbload = len;

    *pcbread = cbload;

    if (path)
        COM_FileBase(path, base);

    buf = (unsigned char*) Hunk_TempAlloc(cbload + 1);
    if (!buf)
    {
        if (path)
            FS_Close(hFile);

        FS_Close(hFile);
        return NULL;
    }

    buf[cbload] = 0;
    FS_Read(buf, cbload, hFile);
    *phFile = hFile;

    return buf;
}
