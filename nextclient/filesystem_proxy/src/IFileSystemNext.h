#pragma once

class IFileSystemNext : public IBaseInterface
{
public:
    // Specifies an alias for the path. In other words, when referring to alias from such operations as Open, FileExists and others, the actual reference will go to path.
    // For all operations where paths are involved, the alias will be resolved first, and only then will all specified serach paths be searched.
    virtual void SetPathAlias(const char* path, const char* alias) = 0;
    virtual bool RemovePathAlias(const char* path) = 0;
};

#define FILESYSTEM_NEXT_INTERFACE_VERSION "NEXT_FILE_SYSTEM_002"