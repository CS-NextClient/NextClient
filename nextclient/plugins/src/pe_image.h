#pragma once
#include "catalog.h"
#include <windows.h>
#include <cstring>

namespace plugins
{
    class PeImage
    {
        const std::vector<unsigned char>& bytes_;
        size_t sections_{};
        static void require(bool valid, const char* token)
        {
            if (!valid)
                throw std::runtime_error(message(token));
        }

    public:
        IMAGE_FILE_HEADER header{};
        IMAGE_OPTIONAL_HEADER32 optional{};
        template <class T>
        T Read(size_t offset) const
        {
            require(offset <= bytes_.size() && sizeof(T) <= bytes_.size() - offset, "#NextPlugins_ErrorPeTruncated");
            T value;
            std::memcpy(&value, bytes_.data() + offset, sizeof(value));
            return value;
        }
        explicit PeImage(const std::vector<unsigned char>& bytes) :
            bytes_(bytes)
        {
            const auto dos = Read<IMAGE_DOS_HEADER>(0);
            require(dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0, "#NextPlugins_ErrorDosHeader");
            const auto offset = static_cast<size_t>(dos.e_lfanew);
            require(Read<DWORD>(offset) == IMAGE_NT_SIGNATURE, "#NextPlugins_ErrorPeSignature");
            header = Read<IMAGE_FILE_HEADER>(offset + sizeof(DWORD));
            require(header.Machine == IMAGE_FILE_MACHINE_I386, "#NextPlugins_ErrorArchitecture");
            require((header.Characteristics & IMAGE_FILE_DLL) != 0, "#NextPlugins_ErrorDll");
            require(header.NumberOfSections <= 96 && header.SizeOfOptionalHeader >= sizeof(optional), "#NextPlugins_ErrorPeHeader");
            optional = Read<IMAGE_OPTIONAL_HEADER32>(offset + sizeof(DWORD) + sizeof(header));
            require(optional.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC, "#NextPlugins_ErrorPe32");
            sections_ = offset + sizeof(DWORD) + sizeof(header) + header.SizeOfOptionalHeader;
            for (size_t i = 0; i < header.NumberOfSections; ++i)
                (void)Section(i);
        }
        IMAGE_SECTION_HEADER Section(size_t index) const
        {
            require(index < header.NumberOfSections, "#NextPlugins_ErrorPeHeader");
            auto section = Read<IMAGE_SECTION_HEADER>(sections_ + index * sizeof(IMAGE_SECTION_HEADER));
            require(
                section.PointerToRawData <= bytes_.size() && section.SizeOfRawData <= bytes_.size() - section.PointerToRawData,
                "#NextPlugins_ErrorPeTruncated"
            );
            return section;
        }
        size_t Offset(uint32_t rva) const
        {
            for (size_t i = 0; i < header.NumberOfSections; ++i)
            {
                const auto section = Section(i);
                if (rva >= section.VirtualAddress && rva - section.VirtualAddress < section.SizeOfRawData)
                    return static_cast<size_t>(section.PointerToRawData) + rva - section.VirtualAddress;
            }
            throw std::runtime_error(message("#NextPlugins_ErrorPeAddress"));
        }
    };
} // namespace plugins
