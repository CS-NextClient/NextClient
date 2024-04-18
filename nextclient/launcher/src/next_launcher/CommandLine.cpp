#include "CommandLine.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

CCommandLine::CCommandLine()
{
}

CCommandLine::~CCommandLine()
{
    delete[] m_pszCmdLine;
}

void CCommandLine::CreateCmdLine(const char* commandline)
{
    assert(commandline != nullptr);

    char szFull[4096];
    char szFileName[4096];

    delete[] m_pszCmdLine;

    size_t iLen = strlen(commandline) + 1;
    char* pszCmdLine = new char[iLen];
    strcpy_s(pszCmdLine, iLen, commandline);

    const char* pszSource = pszCmdLine;
    bool allowAtSign = true;
    char* pszDest = szFull;

    bool bContinue = true;

    while (*pszSource && bContinue)
    {
        while (!allowAtSign || *pszSource != '@')
        {
            *pszDest++ = *pszSource;
            allowAtSign = !!isspace(*pszSource);
            ++pszSource;

            if (!(*pszSource))
            {
                bContinue = false;
                break;
            }
        }

        if (!bContinue)
            break;
        ++pszSource;

        //Get the complete name.
        {
            const char* pszEnd = strchr(pszSource, ' ');
            const size_t uiFileNameLength = pszEnd ? (pszEnd - pszSource) : strlen(pszSource);

            strncpy_s(szFileName, pszSource, std::min(uiFileNameLength, sizeof(szFileName)));
            szFileName[sizeof(szFileName) - 1] = '\0';

            pszSource += uiFileNameLength;
        }

        FILE* pParamFile;
        //Try to open it, if successful, read all options and add them.
        if (fopen_s(&pParamFile, szFileName, "r") == 0)
        {
            while (!feof(pParamFile))
            {
                //Make sure it never reads in too much.
                if (fgets(pszDest, sizeof(szFull) - (pszDest - szFull), pParamFile))
                {
                    const size_t uiLength = strlen(pszDest);
                    //Overwrite the newline with a whitespace.
                    pszDest[uiLength - 1] = ' ';
                    pszDest += uiLength;
                }
            }

            fclose(pParamFile);
        }
        else
        {
            printf("Parameter file '%s' not found, skipping...", szFileName);
        }
    }

    *pszDest = '\0';

    delete[] pszCmdLine;

    iLen = (pszDest - szFull) + 1;
    m_pszCmdLine = new char[iLen];
    strcpy_s(m_pszCmdLine, iLen, szFull);
}

void CCommandLine::CreateCmdLine(int argc, char** argv)
{
    char szFull[4096] = {};

    char* pszDest = szFull;

    for (int i = 0; i < argc; ++i)
    {
        //Add quotes around arguments with spaces.
        if (strchr(argv[i], ' '))
        {
            *pszDest++ = '"';

            strncat_s(pszDest, sizeof(szFull), argv[i], (sizeof(szFull) - 1) - (pszDest - szFull));
            pszDest += strlen(argv[i]);

            *pszDest++ = '"';
        }
        else
        {
            strncat_s(pszDest, sizeof(szFull), argv[i], (sizeof(szFull) - 1) - (pszDest - szFull));
            pszDest += strlen(argv[i]);
        }
    }

    *pszDest = '\0';

    CreateCmdLine(szFull);
}

const char* CCommandLine::GetCmdLine() const
{
    return m_pszCmdLine;
}

const char* CCommandLine::CheckParm(const char* psz, const char** ppszValue) const
{
    if (!m_pszCmdLine)
        return nullptr;

    static char sz[128];

    char* result = strstr(m_pszCmdLine, psz);

    if (result && ppszValue)
    {
        *ppszValue = nullptr;

        char* pszParmStart = result;

        if (*pszParmStart && *pszParmStart != ' ')
        {
            do
            {
                ++pszParmStart;
            } while (*pszParmStart != ' ' && *pszParmStart);
        }

        ++pszParmStart;

        int i;

        for (i = 0; i < (sizeof(sz) - 1) && *pszParmStart && *pszParmStart != ' '; ++i, ++pszParmStart)
        {
            sz[i] = *pszParmStart;
        }

        sz[i] = '\0';

        *ppszValue = sz;
    }

    return result;
}

void CCommandLine::RemoveParm(const char* pszParm)
{
    if (!m_pszCmdLine || !pszParm || !(*pszParm))
        return;

    while (true)
    {
        const size_t uiLength = strlen(m_pszCmdLine);

        char* const pszParmLocation = strstr(m_pszCmdLine, pszParm);

        //No more occurences found; trim trailing whitespace and exit.
        if (!pszParmLocation)
        {
            for (size_t uiIndex = uiLength; uiIndex > 0; uiIndex = strlen(m_pszCmdLine))
            {
                char* pszNext = &m_pszCmdLine[uiIndex - 1];

                if (*pszNext != ' ')
                    break;

                *pszNext = '\0';
            }

            return;
        }

        //Skip the '+' or '-' at the start so it doesn't get stuck looping over itself.
        const char* pszNextStart = pszParmLocation + 1;

        //Find the start of the next command.
        //TODO: this doesn't check if the value is quoted and contains a '+' or '-'. - Solokiller
        while (*pszNextStart && *pszNextStart != '-' && *pszNextStart != '+')
        {
            ++pszNextStart;
        }

        const size_t uiTrailingLength = uiLength - (pszNextStart - m_pszCmdLine);

        //Move the trailing commands (if any) forward, zero out leftover data.
        memmove(pszParmLocation, pszNextStart, uiTrailingLength);
        memset(pszParmLocation + uiTrailingLength, 0, pszNextStart - pszParmLocation);
    }
}

void CCommandLine::AppendParm(const char* pszParm, const char* pszValues)
{
    const size_t uiParmNameLength = strlen(pszParm);
    size_t uiParmLength = uiParmNameLength;

    if (pszValues)
        uiParmLength += strlen(pszValues) + 1;

    if (m_pszCmdLine)
    {
        //Remove any old data for this parameter.
        RemoveParm(pszParm);

        const size_t uiNewLength = uiParmLength + strlen(m_pszCmdLine) + 3; //+ 3 Because whitespace before parm name, between name & values, null terminator.

        char* pszNewCmdLine = new char[uiNewLength];

        if (pszValues)
        {
            snprintf(pszNewCmdLine, uiNewLength, "%s %s %s", m_pszCmdLine, pszParm, pszValues);
        }
        else
        {
            snprintf(pszNewCmdLine, uiNewLength, "%s %s", m_pszCmdLine, pszParm);
        }

        delete[] m_pszCmdLine;
        m_pszCmdLine = pszNewCmdLine;
    }
    else
    {
        int iLen = uiParmLength + 1;
        m_pszCmdLine = new char[iLen];

        strcpy_s(m_pszCmdLine, iLen, pszParm);

        if (pszValues)
        {
            m_pszCmdLine[uiParmNameLength] = ' ';
            m_pszCmdLine[uiParmNameLength + 1] = '\0';
            strcat_s(m_pszCmdLine, iLen, pszValues);
        }
    }
}

void CCommandLine::SetParm(const char* pszParm, const char* pszValues)
{
    //TODO: also called by AppendParm, remove? - Solokiller
    RemoveParm(pszParm);
    AppendParm(pszParm, pszValues);
}

void CCommandLine::SetParm(const char* pszParm, int iValue)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "%d", iValue);

    SetParm(pszParm, buf);
}