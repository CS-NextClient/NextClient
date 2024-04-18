#include "FileSystem_Proxy.h"
#include <strtools.h>
#include <cstdio>
#include <Windows.h>
#include <format>
#include <nitro_utils/string_utils.h>

#undef GetCurrentDirectory

static FileSystem_Proxy g_FileSystem_Proxy;
static FileSystemNext g_FileSystemNext;
static IFileSystem* g_FileSystem_Stdio;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(FileSystem_Proxy, IFileSystem, FILESYSTEM_INTERFACE_VERSION, g_FileSystem_Proxy)
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(FileSystem_Proxy, IFileSystemNext, FILESYSTEM_NEXT_INTERFACE_VERSION, g_FileSystemNext)

void FileSystem_Proxy::Mount()
{
    CSysModule *fs_module = Sys_LoadModule("filesystem_stdio.dll");
    if (fs_module == nullptr)
    {
        MessageBoxA(NULL, "Failed to start the file manager subsystem.\nMake sure that filesystem_stdio.dll is exists in the cs.exe root folder.", ERROR_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    auto fnCreateIFS = Sys_GetFactory(fs_module);
    g_FileSystem_Stdio = (IFileSystem*)fnCreateIFS(FILESYSTEM_INTERFACE_VERSION, 0);
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->Mount();
    else
        MessageBoxA(NULL, "Failed to start the file manager subsystem. Filesystem interface not found.", ERROR_TITLE, MB_OK | MB_ICONERROR);
}

void FileSystem_Proxy::Unmount()
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->Unmount();
}

void FileSystem_Proxy::RemoveAllSearchPaths()
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->RemoveAllSearchPaths();
}

void FileSystem_Proxy::AddSearchPath(const char *pPath, const char *pathID)
{
    if (!g_FileSystem_Stdio)
        return;

    g_FileSystem_Stdio->AddSearchPath(pPath, pathID);
}

// don't use this, bad implementation in FileSystem_Stdio
bool FileSystem_Proxy::RemoveSearchPath(const char *pPath)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->RemoveSearchPath(pPath);

    return false;
}

void FileSystem_Proxy::RemoveFile(const char *pRelativePath, const char *pathID)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pRelativePath);
        g_FileSystem_Stdio->RemoveFile(resolvedPath, pathID);
    }
}

void FileSystem_Proxy::CreateDirHierarchy(const char *path, const char *pathID)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(path);
        g_FileSystem_Stdio->CreateDirHierarchy(resolvedPath, pathID);
    }
}

bool FileSystem_Proxy::FileExists(const char *pFileName)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        return g_FileSystem_Stdio->FileExists(resolvedPath);
    }

    return false;
}

bool FileSystem_Proxy::IsDirectory(const char *pFileName)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        return g_FileSystem_Stdio->IsDirectory(resolvedPath);
    }

    return false;
}

FileHandle_t FileSystem_Proxy::Open(const char *pFileName, const char *pOptions, const char *pathID)
{
    FileHandle_t f;
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        f = g_FileSystem_Stdio->Open(resolvedPath, pOptions, pathID);
        return f;
    }

    return nullptr;
}

void FileSystem_Proxy::Close(FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->Close(file);
}

void FileSystem_Proxy::Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->Seek(file, pos, seekType);
}

unsigned int FileSystem_Proxy::Tell(FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->Tell(file);

    return 0;
}

unsigned int FileSystem_Proxy::Size(FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->Size(file);

    return 0;
}

unsigned int FileSystem_Proxy::Size(const char *pFileName)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        return g_FileSystem_Stdio->Size(resolvedPath);
    }

    return 0;
}

long FileSystem_Proxy::GetFileTime(const char *pFileName)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        return g_FileSystem_Stdio->GetFileTime(resolvedPath);
    }

    return 0;
}

void FileSystem_Proxy::FileTimeToString(char *pStrip, int maxCharsIncludingTerminator, long fileTime)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->FileTimeToString(pStrip, maxCharsIncludingTerminator, fileTime);
}

bool FileSystem_Proxy::IsOk(FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->IsOk(file);

    return false;
}

void FileSystem_Proxy::Flush(FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->Flush(file);
}

bool FileSystem_Proxy::EndOfFile(FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->EndOfFile(file);

    return false;
}

int FileSystem_Proxy::Read(void *pOutput, int size, FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->Read(pOutput, size, file);

    return 0;
}

int FileSystem_Proxy::Write(const void *pInput, int size, FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->Write(pInput, size, file);

    return 0;
}

char *FileSystem_Proxy::ReadLine(char *pOutput, int maxChars, FileHandle_t file)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->ReadLine(pOutput, maxChars, file);

    return nullptr;
}

int FileSystem_Proxy::FPrintf(FileHandle_t file, const char *pFormat, ...)
{
    if (g_FileSystem_Stdio)
    {
        char buf[512];

        va_list args;
            va_start(args, pFormat);
        int symbols = vsnprintf(buf, sizeof(buf), pFormat, args );
            va_end(args);

        if (symbols > sizeof(buf) - 1)
        {
            char* big_buf = new char[symbols + 1];

                va_start(args, pFormat);
            vsnprintf(big_buf, sizeof(big_buf), pFormat, args);
                va_end(args);

            int result = g_FileSystem_Stdio->FPrintf(file, "%s", big_buf);

            delete[] big_buf;

            return result;
        }

        return g_FileSystem_Stdio->FPrintf(file, "%s", buf);
    }

    return 0;
}

void *FileSystem_Proxy::GetReadBuffer(FileHandle_t file, int *outBufferSize, bool failIfNotInCache)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->GetReadBuffer(file, outBufferSize, failIfNotInCache);

    return nullptr;
}

void FileSystem_Proxy::ReleaseReadBuffer(FileHandle_t file, void *readBuffer)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->ReleaseReadBuffer(file, readBuffer);
}

const char *FileSystem_Proxy::FindFirst(const char *pWildCard, FileFindHandle_t *pHandle, const char *pathID)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->FindFirst(pWildCard, pHandle, pathID);

    return nullptr;
}

const char *FileSystem_Proxy::FindNext(FileFindHandle_t handle)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->FindNext(handle);

    return nullptr;
}

bool FileSystem_Proxy::FindIsDirectory(FileFindHandle_t handle)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->FindIsDirectory(handle);

    return false;
}

void FileSystem_Proxy::FindClose(FileFindHandle_t handle)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->FindClose(handle);
}

void FileSystem_Proxy::GetLocalCopy(const char *pFileName)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        g_FileSystem_Stdio->GetLocalCopy(resolvedPath);
    }
}

const char *FileSystem_Proxy::GetLocalPath(const char *pFileName, char *pLocalPath, int localPathBufferSize)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        bool convert = strstr(resolvedPath, "motd_temp.html") != nullptr;

        const char* result = g_FileSystem_Stdio->GetLocalPath(resolvedPath, pLocalPath, localPathBufferSize);
        if (convert)
        {
            wchar_t wlocal_path[MAX_PATH];
            int bytes = MultiByteToWideChar(CP_ACP, 0, pLocalPath, -1, wlocal_path, sizeof(wlocal_path));
            if (bytes != 0)
                Q_UTF16ToUTF8(wlocal_path, pLocalPath, localPathBufferSize);
        }

        return result;
    }

    return nullptr;
}

char *FileSystem_Proxy::ParseFile(char *pFileBytes, char *pToken, bool *pWasQuoted)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->ParseFile(pFileBytes, pToken, pWasQuoted);

    return nullptr;
}

bool FileSystem_Proxy::FullPathToRelativePath(const char *pFullpath, char *pRelative)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->FullPathToRelativePath(pFullpath, pRelative);

    return false;
}

bool FileSystem_Proxy::GetCurrentDirectory(char *pDirectory, int maxlen)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->GetCurrentDirectory(pDirectory, maxlen);

    return false;
}

void FileSystem_Proxy::PrintOpenedFiles(void)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->PrintOpenedFiles();
}

void FileSystem_Proxy::SetWarningFunc(void (*pfnWarning)(const char *, ...))
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->SetWarningFunc(pfnWarning);
}

void FileSystem_Proxy::SetWarningLevel(FileWarningLevel_t level)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->SetWarningLevel(level);
}

void FileSystem_Proxy::LogLevelLoadStarted(const char *name)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->LogLevelLoadStarted(name);
}

void FileSystem_Proxy::LogLevelLoadFinished(const char *name)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->LogLevelLoadFinished(name);
}

int FileSystem_Proxy::HintResourceNeed(const char *hintlist, int forgetEverything)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->HintResourceNeed(hintlist, forgetEverything);

    return 0;
}

int FileSystem_Proxy::PauseResourcePreloading(void)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->PauseResourcePreloading();

    return 0;
}

int FileSystem_Proxy::ResumeResourcePreloading(void)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->ResumeResourcePreloading();

    return 0;
}

int FileSystem_Proxy::SetVBuf(FileHandle_t stream, char *buffer, int mode, long size)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->SetVBuf(stream, buffer, mode, size);

    return 0;
}

void FileSystem_Proxy::GetInterfaceVersion(char *p, int maxlen)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->GetInterfaceVersion(p, maxlen);
}

bool FileSystem_Proxy::IsFileImmediatelyAvailable(const char *pFileName)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->IsFileImmediatelyAvailable(pFileName);

    return false;
}

WaitForResourcesHandle_t FileSystem_Proxy::WaitForResources(const char *resourcelist)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->WaitForResources(resourcelist);

    return 0;
}

bool FileSystem_Proxy::GetWaitForResourcesProgress(WaitForResourcesHandle_t handle, float *progress, bool *complete)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->GetWaitForResourcesProgress(handle, progress, complete);

    return false;
}

void FileSystem_Proxy::CancelWaitForResources(WaitForResourcesHandle_t handle)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->CancelWaitForResources(handle);
}

bool FileSystem_Proxy::IsAppReadyForOfflinePlay(int appID)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->IsAppReadyForOfflinePlay(appID);

    return false;
}

bool FileSystem_Proxy::AddPackFile(const char *fullpath, const char *pathID)
{
    if (g_FileSystem_Stdio)
        return g_FileSystem_Stdio->AddPackFile(fullpath, pathID);

    return false;
}

FileHandle_t FileSystem_Proxy::OpenFromCacheForRead(const char *pFileName, const char *pOptions, const char *pathID)
{
    if (g_FileSystem_Stdio)
    {
        const char* resolvedPath = g_FileSystemNext.ResolveAliasPath(pFileName);
        g_FileSystem_Stdio->OpenFromCacheForRead(resolvedPath, pOptions, pathID);
    }

    return nullptr;
}

void FileSystem_Proxy::AddSearchPathNoWrite(const char *pPath, const char *pathID)
{
    if (g_FileSystem_Stdio)
        g_FileSystem_Stdio->AddSearchPathNoWrite(pPath, pathID);
}

void FileSystemNext::SetPathAlias(const char* path, const char* alias)
{
    std::string alias_s = alias;
    nitro_utils::to_lower(alias_s);

    alias_path_.insert_or_assign(alias_s, path);
}

bool FileSystemNext::RemovePathAlias(const char* path)
{
    std::string path_s = path;
    nitro_utils::to_lower(path_s);

    return alias_path_.erase(path_s);
}

const char* FileSystemNext::ResolveAliasPath(const char* path)
{
    std::string path_s = path;
    nitro_utils::to_lower(path_s);

    if (alias_path_.contains(path_s))
        return alias_path_[path_s].c_str();

    return path;
}
