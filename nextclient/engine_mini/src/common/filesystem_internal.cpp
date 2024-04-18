#include "../engine.h"

#undef GetCurrentDirectory

FileHandle_t FS_Open(const char* pFileName, const char* pOptions)
{
    return g_pFileSystem->Open(pFileName, pOptions);
}

unsigned int FS_Size(FileHandle_t file)
{
    return g_pFileSystem->Size(file);
}

void FS_Close(FileHandle_t file)
{
    g_pFileSystem->Close(file);
}

void FS_Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType)
{
    g_pFileSystem->Seek(file, pos, seekType);
}

bool FS_EndOfFile(FileHandle_t file)
{
    return g_pFileSystem->EndOfFile(file);
}

int FS_Read(void* pOutput, int size, FileHandle_t file)
{
    return g_pFileSystem->Read(pOutput, size, file);
}

void* FS_GetReadBuffer(FileHandle_t file, int* outBufferSize)
{
    return g_pFileSystem->GetReadBuffer(file, outBufferSize, true);
}

void FS_ReleaseReadBuffer(FileHandle_t file, void* readBuffer)
{
    g_pFileSystem->ReleaseReadBuffer(file, readBuffer);
}

void FS_RemoveFile(const char* pRelativePath, const char* pathID)
{
    g_pFileSystem->RemoveFile(pRelativePath, pathID);
}

void FS_CreateDirHierarchy(const char* path, const char* pathID)
{
    g_pFileSystem->CreateDirHierarchy(path, pathID);
}

bool FS_FileExists(const char* pFileName)
{
    return g_pFileSystem->FileExists(pFileName);
}

unsigned int FS_Tell(FileHandle_t file)
{
    return g_pFileSystem->Tell(file);
}

int FS_FileSize(const char* pFileName)
{
    return g_pFileSystem->Size(pFileName);
}

const char* FS_GetLocalPath(const char* pFileName, char* pLocalPath, int localPathBufferSize)
{
    return g_pFileSystem->GetLocalPath(pFileName, pLocalPath, localPathBufferSize);
}

bool FS_GetCurrentDirectory(char* pDirectory, int maxlen)
{
    return g_pFileSystem->GetCurrentDirectory(pDirectory, maxlen);
}

void FS_LogLevelLoadStarted(const char* name)
{
    return g_pFileSystem->LogLevelLoadStarted(name);
}
