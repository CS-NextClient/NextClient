#pragma once
#include "icommandline.h"

class CCommandLine : public ICommandLine
{
    char* m_pszCmdLine = nullptr;

public:
    CCommandLine();
    ~CCommandLine();

    void CreateCmdLine(const char* commandline) override;
    void CreateCmdLine(int argc, char **argv) override;
    const char* GetCmdLine() const override;

    const char* CheckParm(const char* psz, const char** ppszValue = nullptr) const override;
    void RemoveParm(const char* pszParm) override;
    void AppendParm(const char* pszParm, const char* pszValues) override;

    int ParmValue(const char *psz, int nDefaultVal) const override { return 0; };
    float ParmValue(const char *psz, float flDefaultVal) const override { return 0; };
    const char *ParmValue(const char *psz, const char *pDefaultVal) const override { return nullptr; };

    int ParmCount() const override { return 0; };
    int FindParm(const char *psz) const override { return 0; };
    const char *GetParm(int nIndex) const override { return nullptr; };

    void SetParm(const char* pszParm, const char* pszValues);
    void SetParm(const char* pszParm, int iValue);
};