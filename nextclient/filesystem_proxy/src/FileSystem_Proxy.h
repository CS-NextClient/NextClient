#pragma once

#include <FileSystem.h>
#include "IFileSystemNext.h"
#include <unordered_map>
#include <string>

class FileSystem_Proxy : public IFileSystem
{
public:
    void Mount(void) override;
    void Unmount(void) override;
    void RemoveAllSearchPaths(void) override;
    void AddSearchPath(const char *pPath, const char *pathID) override;
    bool RemoveSearchPath(const char *pPath) override;
    void RemoveFile(const char *pRelativePath, const char *pathID) override;
    void CreateDirHierarchy(const char *path, const char *pathID) override;
    bool FileExists(const char *pFileName) override;
    bool IsDirectory(const char *pFileName) override;
    FileHandle_t Open(const char *pFileName, const char *pOptions, const char *pathID) override;
    void Close(FileHandle_t file) override;
    void Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType) override;
    unsigned int Tell(FileHandle_t file) override;
    unsigned int Size(FileHandle_t file) override;
    unsigned int Size(const char *pFileName) override;
    long GetFileTime(const char *pFileName) override;
    void FileTimeToString(char *pStrip, int maxCharsIncludingTerminator, long fileTime) override;
    bool IsOk(FileHandle_t file) override;
    void Flush(FileHandle_t file) override;
    bool EndOfFile(FileHandle_t file) override;
    int Read(void *pOutput, int size, FileHandle_t file) override;
    int Write(const void *pInput, int size, FileHandle_t file) override;
    char *ReadLine(char *pOutput, int maxChars, FileHandle_t file) override;
    int FPrintf(FileHandle_t file, const char *pFormat, ...) override;
    void *GetReadBuffer(FileHandle_t file, int *outBufferSize, bool failIfNotInCache) override;
    void ReleaseReadBuffer(FileHandle_t file, void *readBuffer) override;
    const char *FindFirst(const char *pWildCard, FileFindHandle_t *pHandle, const char *pathID) override;
    const char *FindNext(FileFindHandle_t handle) override;
    bool FindIsDirectory(FileFindHandle_t handle) override;
    void FindClose(FileFindHandle_t handle) override;
    void GetLocalCopy(const char *pFileName) override;
    const char *GetLocalPath(const char *pFileName, char *pLocalPath, int localPathBufferSize) override;
    char *ParseFile(char *pFileBytes, char *pToken, bool *pWasQuoted) override;
    bool FullPathToRelativePath(const char *pFullpath, char *pRelative) override;
    bool GetCurrentDirectory(char *pDirectory, int maxlen) override;
    void PrintOpenedFiles(void) override;
    void SetWarningFunc(void (*pfnWarning)(const char *, ...)) override;
    void SetWarningLevel(FileWarningLevel_t level) override;
    void LogLevelLoadStarted(const char *name) override;
    void LogLevelLoadFinished(const char *name) override;
    int HintResourceNeed(const char *hintlist, int forgetEverything) override;
    int PauseResourcePreloading(void) override;
    int ResumeResourcePreloading(void) override;
    int SetVBuf(FileHandle_t stream, char *buffer, int mode, long size) override;
    void GetInterfaceVersion(char *p, int maxlen) override;
    bool IsFileImmediatelyAvailable(const char *pFileName) override;
    WaitForResourcesHandle_t WaitForResources(const char *resourcelist) override;
    bool GetWaitForResourcesProgress(WaitForResourcesHandle_t handle, float *progress, bool *complete) override;
    void CancelWaitForResources(WaitForResourcesHandle_t handle) override;
    bool IsAppReadyForOfflinePlay(int appID) override;
    bool AddPackFile(const char *fullpath, const char *pathID) override;
    FileHandle_t OpenFromCacheForRead(const char *pFileName, const char *pOptions, const char *pathID) override;
    void AddSearchPathNoWrite(const char *pPath, const char *pathID) override;
};

class FileSystemNext : public IFileSystemNext
{
    std::unordered_map<std::string, std::string> alias_path_;

public:
    void SetPathAlias(const char* path, const char* alias) override;
    bool RemovePathAlias(const char* path) override;

    // If the passed path is an alias, returns the path that matches the alias.
    // If the passed path is not an alias, returns it as is.
    const char* ResolveAliasPath(const char* path);
};