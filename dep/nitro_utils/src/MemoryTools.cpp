#define NMD_ASSEMBLY_IMPLEMENTATION
#include <cstring>
#include <nitro_utils/MemoryTools.h>

using namespace std;
using namespace nitro_utils;

std::string trim(std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && isspace(*it))
        it++;

    std::string::const_reverse_iterator rit = s.rbegin();
    while (rit.base() != it && isspace(*rit))
        rit++;

    return s = std::string(it, rit.base());
}

size_t closing_symbol(string str, size_t pos, char close)
{
    char open = str[pos]; int level = 1;

    for (size_t i = pos + 1; i < str.length(); i++)
    {
        if (str[i] == open)
        {
            if (open == close) return i;
            else level++;
        }
        else if (str[i] == close)
        {
            if (!--level)
                return i;
        }
    }
    return string::npos;
}

vector<string> split(string s, string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

namespace utils
{
    static uint32_t RelativeToAbsolute(uint32_t dwAddress)
    {
        ptrdiff_t dwOffset = *(uint32_t*)dwAddress;
        return (dwAddress + dwOffset + sizeof(uint32_t));
    }
    static uint32_t AbsoluteToRelative(uint32_t dwAddress, uint32_t dwAbsRef)
    {
        return dwAbsRef - dwAddress - sizeof(uint32_t);
    }
}

MemScanner::MemScanner()
{

}

MemScanner::MemScanner(const MemoryModule &memModule)
{
    SetContext(memModule);
}

bool MemScanner::SetContext(const MemoryModule &memModule)
{
    if (memModule.Start())
    {
        m_dwFindStart = memModule.Start();
        m_dwFindEnd = memModule.End();
        m_hModule = memModule.Module();
        return true;
    }
    return false;
}

uint32_t MemScanner::ModuleStart() const
{
    return m_dwFindStart;
}

uint32_t MemScanner::ModuleEnd() const
{
    return m_dwFindEnd;
}

bool MemScanner::AddrInRange(uint32_t dwAddress, uint32_t dwLB, uint32_t dwHB) const
{
    if (!dwLB) dwLB = ModuleStart();
    if (!dwHB) dwHB = ModuleEnd();

    return (dwAddress >= dwLB) && (dwAddress <= dwHB);
}

size_t MemScanner::AOBToMaskedPattern(const char* pszPattern, memorypattern_t* pPattern)
{
    vector<string> vecBytes = split(pszPattern, " ");
    if (!vecBytes.size()) return 0;

    uint32_t ulPatternLen = 0;
    for (auto sByte : vecBytes)
    {
        switch (sByte[0])
        {
            case '?':
            {
                pPattern->m_vecPattern.push_back(0);
                pPattern->m_vecMask.push_back(0);
                ulPatternLen++;
                break;
            }
            case '-':
            case '+':
            {
                pPattern->m_iDiff += stoi(sByte.substr(1)) * (sByte[0] == '-' ? -1 : 1);
                break;
            }
            default:
            {
                pPattern->m_vecPattern.push_back((uint8_t)(stoul(sByte, nullptr, 16) & 0xFF));
                pPattern->m_vecMask.push_back(1);
                ulPatternLen++;
                break;
            }
        }
    }
    return ulPatternLen;
}

size_t MemScanner::StringToMaskedPattern(const char* pszString, memorypattern_t* pPattern, bool bEOS)
{
    uint32_t ulPatternLen = 0;
    size_t iLen = strlen(pszString);
    if (!bEOS) iLen--;

    while (ulPatternLen <= iLen)
    {
        pPattern->m_vecPattern.push_back((uint8_t)pszString[ulPatternLen]);
        pPattern->m_vecMask.push_back(1);
        ulPatternLen++;
    }

    return ulPatternLen;
}

uint32_t MemScanner::FindString(const char* pszString, bool bEOS, uint32_t dwStart, int32_t iRange)
{
    memorypattern_t mempPattern;
    if (!StringToMaskedPattern(pszString, &mempPattern, bEOS)) return 0;

    return FindPattern(&mempPattern, dwStart, iRange);
}

uint32_t MemScanner::FindPattern(const char* pszPattern, uint32_t dwStart, int32_t iRange)
{
    memorypattern_t mempPattern;
    if(!AOBToMaskedPattern(pszPattern, &mempPattern)) return 0;

    return FindPattern(&mempPattern, dwStart, iRange);
}

uint32_t MemScanner::FindPattern(memorypattern_t* pPattern, uint32_t dwStart, int32_t iRange) const
{
    if (dwStart == 0) dwStart = ModuleStart();
    if (!AddrInRange(dwStart) || !AddrInRange(dwStart + iRange)) return 0;

    uint32_t dwResult = 0;
    int8_t bDir = iRange >= 0 ? 1 : -1;
    size_t ulPatternLength = pPattern->m_vecMask.size();

    for (uint32_t dwAddr = dwStart; AddrInRange(dwAddr + ulPatternLength * bDir); dwAddr += bDir)
    {
        bool bFound = true;
        int iSize = (int)pPattern->m_vecMask.size();
        if (bDir > 0)
        {
            if (iRange && dwAddr > dwStart + iRange) return 0;

            for (int i = 0; i < iSize; i++)
            {
                if (pPattern->m_vecPattern[i] != *(uint8_t*)(dwAddr + i) && pPattern->m_vecMask[i])
                {
                    bFound = false;
                    break;
                }
            }
        }
        else
        {
            if (iRange && dwAddr < dwStart + iRange) return 0;

            for (int i = iSize - 1; i >= 0; i--)
            {
                if (pPattern->m_vecPattern[i] != *(uint8_t*)(dwAddr + i) && pPattern->m_vecMask[i])
                {
                    bFound = false;
                    break;
                }
            }
        }
        if (bFound)
        {
            dwResult = dwAddr;
            break;
        }
    }

    dwResult += pPattern->m_iDiff;
    if (!AddrInRange(dwResult)) return 0;

    return dwResult;
}

uint32_t MemScanner::FindPattern2(string sPattern, uint32_t dwStart) const
{
    uint8_t pszBuffer[512] = { 0 }, pszMask[512] = { 0 };
    int32_t lRange = 0, lOffset = 0;
    size_t ulResultNum = 0, ulBufferLength = 0;
    static char cOpEnd[255];
    cOpEnd['('] = ')';
    cOpEnd['['] = ']';
    cOpEnd['{'] = '}';
    cOpEnd['<'] = '>';
    cOpEnd['\''] = '\'';

    for (size_t i = 0; i < sPattern.length(); i++)
    {
        switch (sPattern[i])
        {
            case '{':
            case '[':
            case '(':
            case '<':
            case '\'':
            {
                size_t ulBracketPos = closing_symbol(sPattern, i, cOpEnd[sPattern[i]]);
                if (ulBracketPos == string::npos) return 0;
                string sExp = sPattern.substr(i + 1, ulBracketPos - i - 1);

                if (sExp.length())
                {
                    switch (sPattern[i])
                    {
                        case '{':
                        {
                            uint32_t dwAddress = FindPattern2(sExp, dwStart);
                            if (!dwAddress) return 0;

                            try
                            {
                                *(uint32_t*)&pszBuffer[ulBufferLength] = utils::RelativeToAbsolute(dwAddress);
                            }
                            catch (int a)
                            {
                                throw ;
                            }

                            memset(&pszMask[ulBufferLength], 1, 4);
                            ulBufferLength += 4;

                            break;
                        }
                        case '[':
                        {
                            uint32_t dwAddress = FindPattern2(sExp, dwStart);
                            if (!dwAddress) return 0;

                            try
                            {
                                *(uint32_t*)&pszBuffer[ulBufferLength] = *(uint32_t*)dwAddress;
                            }
                            catch (int a)
                            {
                                throw;
                            }

                            *(uint32_t*)&pszBuffer[ulBufferLength] = *(uint32_t*)dwAddress;
                            memset(&pszMask[ulBufferLength], 1, 4);
                            ulBufferLength += 4;

                            break;
                        }
                        case '<':
                        {
                            uint32_t dwAddress = FindPattern2(sExp, dwStart);
                            if (!dwAddress) return 0;

                            *(uint32_t*)&pszBuffer[ulBufferLength] = dwAddress;
                            memset(&pszMask[ulBufferLength], 1, 4);
                            ulBufferLength += 4;

                            break;
                        }
                        case '(':
                        {
                            uint32_t dwAddress;
                            vector<string> vecArgs = split(sExp, ",");
                            if (vecArgs.size())
                            {
                                dwAddress = FindPattern2(vecArgs[0], dwStart);
                                if (!dwAddress) return 0;

                                dwStart = dwAddress;
                                if (vecArgs.size() == 2)
                                {
                                    trim(vecArgs[1]);
                                    if (vecArgs[1].size() == 1)
                                    {
                                        if (vecArgs[1][0] == '-') lRange = ModuleStart() - dwStart;
                                        else if (vecArgs[1][0] == '+') lRange = dwStart - ModuleStart();
                                    }
                                    else lRange = stoi(vecArgs[1]);
                                }
                            }
                            break;
                        }
                        case '\'':
                        {
                            memcpy(&pszBuffer[ulBufferLength], sExp.c_str(), sExp.length());
                            memset(&pszMask[ulBufferLength], 1, sExp.length());
                            ulBufferLength += sExp.length();

                            break;
                        }
                    }
                    i += sExp.length() + 1;
                }
                break;
            }
            case '-':
            case '+':
            case '#':
            case '*':
            {
                size_t ulSpacePos = sPattern.find_first_of(" \t\r\n", i);
                string sExp = sPattern.substr(i + 1, ulSpacePos - i - 1);

                switch (sPattern[i])
                {
                    case '#':
                    {
                        if (sExp[0] == 'L') ulResultNum = -1;
                        else if (sExp[0] == 'F') ulResultNum = 0;
                        else ulResultNum = stoi(sExp);

                        break;
                    }
                    case '-':
                    case '+':
                    {
                        size_t ulSpacePos = sPattern.find_first_of(" \t\r\n", i);
                        string sExp = sPattern.substr(i, ulSpacePos - i);
                        if (isdigit(sExp[1])) lOffset = stoi(sExp);
                        else
                        {
                            if (sPattern[i] == '-') lOffset = ModuleStart() - dwStart;
                            else if (sPattern[i] == '+') lOffset = dwStart - ModuleStart();
                        }

                        break;
                    }
                }
                break;
            }
            case '&':
            {
                return *(uint32_t*)&pszBuffer[0] + lOffset;
            }
            case '?':
            {
                pszBuffer[ulBufferLength] = '\xFF';
                pszMask[ulBufferLength] = 0;
                ulBufferLength++;
                if (i + 1 < sPattern.length() && sPattern[i] == '?') i++;

                break;
            }
            default:
            {
                if (isspace(sPattern[i]))
                    continue;

                size_t ulSpacePos = sPattern.find_first_of(" \t\r\n", i);
                string sExp = sPattern.substr(i, ulSpacePos - i);
                if (sExp.length() == 2)
                {
                    pszBuffer[ulBufferLength] = (uint8_t)(stoul(sExp, nullptr, 16) & 0xFF);
                    pszMask[ulBufferLength] = 1;
                    ulBufferLength++; i++;
                }

                break;
            }
        }
    }
    if (ulBufferLength)
    {
        while (pszMask[ulBufferLength - 1] == 0) ulBufferLength--;

        uint32_t dwResult = 0;
        int8_t bDir = lRange >= 0 ? 1 : -1;
        int iSize = (int)ulBufferLength;

        if (dwStart == 0) dwStart = ModuleStart();
        if (!AddrInRange(dwStart) || !AddrInRange(dwStart + ulBufferLength * bDir)) return 0;

        uint32_t dwAddr, bFound;
        for (dwAddr = dwStart; AddrInRange(dwAddr + ulBufferLength * bDir); dwAddr += bDir)
        {
            bFound = true;

            if (bDir > 0)
            {
                if (lRange && dwAddr > dwStart + lRange) break;

                for (int i = 0; i < iSize; i++)
                {
                    if (pszMask[i] && pszBuffer[i] != *(uint8_t*)(dwAddr + i))
                    {
                        bFound = false;
                        break;
                    }
                }
            }
            else
            {
                if (lRange && dwAddr < dwStart + lRange) break;

                for (int i = 0, iPos; i < iSize; i++)
                {
                    iPos = iSize - 1 - i;
                    if (pszMask[iPos] && pszBuffer[iPos] != *(uint8_t*)(dwAddr - i))
                    {
                        bFound = false;
                        break;
                    }
                }
            }
            if (bFound)
            {
                dwResult = dwAddr;
                if (bDir < 0) dwResult -= ulBufferLength - 1;

                if (ulResultNum != -1)
                {
                    if (ulResultNum) ulResultNum--;
                    else break;
                }
            }
        }

        dwResult += lOffset;
        if (!AddrInRange(dwResult)) return 0;

        return dwResult;
    }

    return 0;
}
