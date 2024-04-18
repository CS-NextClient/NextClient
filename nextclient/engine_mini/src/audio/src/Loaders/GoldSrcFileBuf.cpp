
#include "Loaders/GoldSrcFileBuf.hpp"
#include "../../../common/filesystem.h"

namespace MetaAudio
{
  GoldSrcFileBuf::int_type GoldSrcFileBuf::underflow()
  {
    if (mFile && gptr() == egptr())
    {
      auto got = FS_Read(mBuffer.data(), mBuffer.size(), mFile);
      if (got)
      {
        setg(mBuffer.data(), mBuffer.data(), mBuffer.data() + got);
      }
    }

    if (gptr() == egptr())
    {
      return traits_type::eof();
    }
    return traits_type::to_int_type(*gptr());
  }

  GoldSrcFileBuf::pos_type GoldSrcFileBuf::seekoff(off_type offset, std::ios_base::seekdir whence, std::ios_base::openmode mode)
  {
    if (!mFile || (mode & std::ios_base::out) || !(mode & std::ios_base::in))
    {
      return traits_type::eof();
    }

    if (offset < 0)
    {
      return traits_type::eof();
    }

    auto seekType = FILESYSTEM_SEEK_HEAD;
    switch (whence)
    {
    case std::ios_base::beg:
      break;

    case std::ios_base::cur:
      seekType = FILESYSTEM_SEEK_CURRENT;
      if ((offset >= 0 && offset < off_type(egptr() - gptr())) ||
        (offset < 0 && -offset <= off_type(gptr() - eback())))
      {
        auto initialPos = FS_Tell(mFile);
        FS_Seek(mFile, static_cast<int>(offset), seekType);
        auto newPos = FS_Tell(mFile);
        if (newPos - initialPos != offset)
        {
          return traits_type::eof();
        }
        setg(eback(), gptr() + offset, egptr());
        return newPos - off_type(egptr() - gptr());
      }
      offset -= off_type(egptr() - gptr());
      break;

    case std::ios_base::end:
      offset += FS_Size(mFile);
      break;

    default:
      return traits_type::eof();
    }

    FS_Seek(mFile, static_cast<int>(offset), seekType);
    auto curPosition = FS_Tell(mFile);

    setg(nullptr, nullptr, nullptr);
    return curPosition;
  }

  GoldSrcFileBuf::pos_type GoldSrcFileBuf::seekpos(pos_type pos, std::ios_base::openmode mode)
  {
    if (!mFile || (mode & std::ios_base::out) || !(mode & std::ios_base::in))
    {
      return traits_type::eof();
    }

    FS_Seek(mFile, static_cast<int>(pos), FILESYSTEM_SEEK_HEAD);
    if (FS_EndOfFile(mFile))
    {
      return traits_type::eof();
    }
    auto curPosition = FS_Tell(mFile);

    setg(nullptr, nullptr, nullptr);
    return curPosition;
  }

  bool GoldSrcFileBuf::open(const char* filename) noexcept
  {
    mFile = FS_Open(filename, "rb");
    return mFile;
  }

  GoldSrcFileBuf::~GoldSrcFileBuf()
  {
    FS_Close(mFile);
    mFile = nullptr;
  }
}
