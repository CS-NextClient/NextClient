#include "CommandLine.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
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

    std::string szFull;
    const char* pszSource = commandline;
    bool allowAtSign = true;

    while (*pszSource)
    {
        if (allowAtSign && *pszSource == '@')
        {
            ++pszSource;

            //Get the complete name.
            const char* pszEnd = strchr(pszSource, ' ');
            const size_t uiFileNameLength = pszEnd ? (size_t)(pszEnd - pszSource) : strlen(pszSource);

            std::string szFileName(pszSource, uiFileNameLength);
            pszSource += uiFileNameLength;

            FILE* pParamFile;
            //Try to open it, if successful, read all options and add them.
            if (fopen_s(&pParamFile, szFileName.c_str(), "r") == 0)
            {
                char szLine[1024];
                while (fgets(szLine, sizeof(szLine), pParamFile))
                {
                    const size_t uiLength = strlen(szLine);
                    //Overwrite the newline with a whitespace.
                    if (uiLength > 0 && szLine[uiLength - 1] == '\n')
                        szLine[uiLength - 1] = ' ';

                    szFull += szLine;
                }

                fclose(pParamFile);
            }
            else
            {
                printf("Parameter file '%s' not found, skipping...", szFileName.c_str());
            }

            allowAtSign = false;
            continue;
        }

        szFull += *pszSource;
        allowAtSign = !!isspace((unsigned char)*pszSource);
        ++pszSource;
    }

    const size_t iLen = szFull.size() + 1;
    char* result = new char[iLen];
    strcpy_s(result, iLen, szFull.c_str());

    delete[] m_pszCmdLine;
    m_pszCmdLine = result;
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

    size_t uiParmNameLength = strlen(pszParm);
    char* pszSearch = m_pszCmdLine;

    while (true)
    {
        char* pszParmLocation = strstr(pszSearch, pszParm);

        //No more occurences found; trim trailing whitespace and exit.
        if (!pszParmLocation)
        {
            size_t uiLength = strlen(m_pszCmdLine);
            while (uiLength > 0 && m_pszCmdLine[uiLength - 1] == ' ')
                m_pszCmdLine[--uiLength] = '\0';

            return;
        }

        //Match only a whole parameter token, not a prefix, so "-full" doesn't hit "-fulldump".
        char chBefore = pszParmLocation == m_pszCmdLine ? ' ' : pszParmLocation[-1];
        char chAfter = pszParmLocation[uiParmNameLength];

        if (chBefore != ' ' || (chAfter != ' ' && chAfter != '\0'))
        {
            pszSearch = pszParmLocation + 1;
            continue;
        }

        //Find the start of the next command; the parameter's value(s) are removed with it.
        //TODO: this doesn't check if the value is quoted and contains a '+' or '-'. - Solokiller
        char* pszNextStart = pszParmLocation + uiParmNameLength;
        while (*pszNextStart && *pszNextStart != '-' && *pszNextStart != '+')
            ++pszNextStart;

        //Move the trailing commands (if any) forward, including the null terminator.
        memmove(pszParmLocation, pszNextStart, strlen(pszNextStart) + 1);
        pszSearch = pszParmLocation;
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