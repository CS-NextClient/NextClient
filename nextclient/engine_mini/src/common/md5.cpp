#include <engine.h>
#include <md5.h>

BOOL MD5_Hash_File(unsigned char digest[16], char* pszFileName, BOOL bUsefopen, BOOL bSeed, unsigned int seed[4])
{
    return eng()->MD5_Hash_File.InvokeChained(digest, pszFileName, bUsefopen, bSeed, seed);
}
