#pragma once

#include <FileSystem.h>

FileHandle_t FS_Open(const char* pFileName, const char* pOptions);
unsigned int FS_Size(FileHandle_t file);
void FS_Close(FileHandle_t file);
void FS_Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType);
bool FS_EndOfFile(FileHandle_t file);
int FS_Read(void* pOutput, int size, FileHandle_t file);
void* FS_GetReadBuffer(FileHandle_t file, int* outBufferSize);
void FS_ReleaseReadBuffer(FileHandle_t file, void* readBuffer);
void FS_RemoveFile(const char* pRelativePath, const char* pathID);
void FS_CreateDirHierarchy(const char* path, const char* pathID);
bool FS_FileExists(const char* pFileName);
unsigned int FS_Tell(FileHandle_t file);
int FS_FileSize(const char* pFileName);
const char* FS_GetLocalPath(const char* pFileName, char* pLocalPath, int localPathBufferSize);
bool FS_GetCurrentDirectory(char* pDirectory, int maxlen);
void FS_LogLevelLoadStarted(const char* name);
