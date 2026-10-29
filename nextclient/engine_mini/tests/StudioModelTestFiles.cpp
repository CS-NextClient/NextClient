#include "StudioModelTestSupport.h"

#include <algorithm>
#include <cstring>

#include <gtest/gtest.h>

#include "common/filesystem.h"

std::unordered_map<std::string, std::string> studio_test::g_Files;
studio_test::FileReadProbe* studio_test::g_FileReadProbe;
using studio_test::g_Files;

FileHandle_t FS_Open(const char* path, const char*)
{
    auto found = g_Files.find(path);
    if (studio_test::g_FileReadProbe != nullptr && studio_test::g_FileReadProbe->path == path)
    {
        studio_test::g_FileReadProbe->opens++;
        if (studio_test::g_FileReadProbe->open_failed)
        {
            return nullptr;
        }

        studio_test::g_FileReadProbe->opened = found != g_Files.end() ? &found->second : nullptr;
    }
    return found != g_Files.end() ? reinterpret_cast<FileHandle_t>(&found->second) : nullptr;
}

unsigned int FS_Size(FileHandle_t file)
{
    if (studio_test::g_FileReadProbe != nullptr && studio_test::g_FileReadProbe->opened == file)
    {
        studio_test::g_FileReadProbe->sizes++;
        if (studio_test::g_FileReadProbe->size_override)
        {
            return *studio_test::g_FileReadProbe->size_override;
        }
    }
    return static_cast<unsigned int>(reinterpret_cast<const std::string*>(file)->size());
}

void FS_Close(FileHandle_t file)
{
    if (studio_test::g_FileReadProbe != nullptr && studio_test::g_FileReadProbe->opened == file)
    {
        studio_test::g_FileReadProbe->closes++;
    }
}

void FS_Seek(FileHandle_t, int, FileSystemSeek_t)
{
    ADD_FAILURE() << "No disk file is open in the capture fixture";
}

bool FS_EndOfFile(FileHandle_t)
{
    return true;
}

int FS_Read(void* output, int size, FileHandle_t file)
{
    if (studio_test::g_FileReadProbe != nullptr && studio_test::g_FileReadProbe->opened == file)
    {
        studio_test::g_FileReadProbe->reads++;
    }
    const std::string& content = *reinterpret_cast<const std::string*>(file);
    const int count = std::min(size, static_cast<int>(content.size()));
    std::memcpy(output, content.data(), count);
    if (studio_test::g_FileReadProbe != nullptr && studio_test::g_FileReadProbe->opened == file && studio_test::g_FileReadProbe->after_read)
    {
        studio_test::g_FileReadProbe->after_read();
    }
    return count;
}

void* FS_GetReadBuffer(FileHandle_t, int*)
{
    return nullptr;
}

void FS_ReleaseReadBuffer(FileHandle_t, void*) {}
void FS_RemoveFile(const char*, const char*) {}
void FS_CreateDirHierarchy(const char*, const char*) {}

bool FS_FileExists(const char* path)
{
    return g_Files.contains(path);
}

unsigned int FS_Tell(FileHandle_t)
{
    return 0;
}

int FS_FileSize(const char*)
{
    return -1;
}

const char* FS_GetLocalPath(const char*, char*, int)
{
    return nullptr;
}

bool FS_GetCurrentDirectory(char*, int)
{
    return false;
}

void FS_LogLevelLoadStarted(const char*) {}
