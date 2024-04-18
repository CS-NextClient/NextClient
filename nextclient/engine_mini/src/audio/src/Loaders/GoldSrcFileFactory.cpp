#include <fstream>

#include "Loaders/GoldSrcFileFactory.hpp"
#include "Loaders/GoldSrcFileStream.hpp"
#include "../../common/filesystem.h"

namespace MetaAudio
{
  alure::UniquePtr<std::istream> GoldSrcFileFactory::openFile(const alure::String& name) noexcept
  {
    alure::String namebuffer = "sound";

    if (name[0] != '/')
    {
      namebuffer.append("/");
    }

    namebuffer.append(name);

    auto fileExists = FS_FileExists(namebuffer.c_str());
    if (!fileExists)
    {
      namebuffer.clear();
      if (name[0] != '/')
      {
        namebuffer.append("/");
      }
      namebuffer.append(name);

      fileExists = FS_FileExists(namebuffer.c_str());
    }

    alure::UniquePtr<std::istream> file;
    if (fileExists)
    {
      char final_file_path[260]; // MAX_PATH
      FS_GetLocalPath(namebuffer.c_str(), final_file_path, sizeof(final_file_path));
      file = alure::MakeUnique<std::ifstream>(final_file_path, std::ios::binary);
      if (file->fail())
      {
        file = alure::MakeUnique<GoldSrcFileStream>(namebuffer.c_str());
        if (file->fail())
        {
          file = nullptr;
        }
        else
        {
          *file >> std::noskipws;
        }
      }
    }

    return std::move(file);
  }
}