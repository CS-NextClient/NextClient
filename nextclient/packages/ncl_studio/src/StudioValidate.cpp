#include "ncl_studio/StudioValidate.h"

#include <cstdint>
#include <string>

#include <goldsrc/format/studio.h>

#include "HeaderShape.h"

namespace ncl_studio
{
    namespace
    {
        // The palette that follows an 8 bit texture's pixels.
        constexpr int64_t kPaletteBytes = 256 * 3;

        bool Fits(int64_t offset, int64_t count, int64_t element_size, int64_t limit)
        {
            if (offset < 0 || count < 0)
            {
                return false;
            }

            return offset + count * element_size <= limit;
        }

        std::string Outside(const std::string& what, int64_t offset, int64_t count, int64_t limit)
        {
            return what + " lies outside the model: offset " + std::to_string(offset) +
                ", count " + std::to_string(count) + ", limit " + std::to_string(limit);
        }
    }

    size_t CachedStudioSize(const void* data, size_t size)
    {
        if (data == nullptr || size < sizeof(goldsrc::studiohdr_t))
        {
            return 0;
        }

        const goldsrc::studiohdr_t* header = static_cast<const goldsrc::studiohdr_t*>(data);

        int64_t cached = header->textureindex != 0 ? header->texturedataindex : header->length;

        if (cached < static_cast<int64_t>(sizeof(goldsrc::studiohdr_t)) || cached > static_cast<int64_t>(size))
        {
            return 0;
        }

        return static_cast<size_t>(cached);
    }

    ncl_utils::Result<> ValidateStudioFile(const void* data, size_t size)
    {
        if (data == nullptr || size < sizeof(goldsrc::studiohdr_t))
        {
            return ncl_utils::ResultError("file is shorter than a studio header");
        }

        const goldsrc::studiohdr_t* header = static_cast<const goldsrc::studiohdr_t*>(data);

        std::string shape_error = HeaderShapeError(header);
        if (!shape_error.empty())
        {
            return ncl_utils::ResultError(std::move(shape_error));
        }

        // The bound for everything below. A table that lies past it survives the load as
        // whatever happens to follow the allocation, which is the whole point of checking.
        int64_t cached = static_cast<int64_t>(CachedStudioSize(data, size));

        if (cached == 0)
        {
            return ncl_utils::ResultError(
                "declared length " + std::to_string(header->length) + " and texture data offset " +
                std::to_string(header->texturedataindex) + " do not fit a file of " + std::to_string(size) + " bytes"
            );
        }

        if (!Fits(header->boneindex, header->numbones, sizeof(goldsrc::mstudiobone_t), cached))
        {
            return ncl_utils::ResultError(Outside("bone array", header->boneindex, header->numbones, cached));
        }

        if (!Fits(header->bodypartindex, header->numbodyparts, sizeof(goldsrc::mstudiobodyparts_t), cached))
        {
            return ncl_utils::ResultError(
                Outside("body part array", header->bodypartindex, header->numbodyparts, cached)
            );
        }

        // Read as short[numskinfamilies][numskinref], and indexed by the entity's skin, so
        // the whole rectangle has to be there and not just the first row.
        if (!Fits(header->skinindex, static_cast<int64_t>(header->numskinref) * header->numskinfamilies,
                  sizeof(short), cached))
        {
            return ncl_utils::ResultError(
                Outside("skin table", header->skinindex,
                    static_cast<int64_t>(header->numskinref) * header->numskinfamilies, cached)
            );
        }

        if (!Fits(header->textureindex, header->numtextures, sizeof(goldsrc::mstudiotexture_t), cached))
        {
            return ncl_utils::ResultError(
                Outside("texture array", header->textureindex, header->numtextures, cached)
            );
        }

        if (header->textureindex == 0)
        {
            // Textures live in a companion file, so there is nothing further to bound here.
            return {};
        }

        const goldsrc::mstudiotexture_t* textures = reinterpret_cast<const goldsrc::mstudiotexture_t*>(
            static_cast<const uint8_t*>(data) + header->textureindex
        );

        for (int i = 0; i < header->numtextures; i++)
        {
            const goldsrc::mstudiotexture_t& texture = textures[i];

            if (texture.width <= 0 || texture.height <= 0)
            {
                return ncl_utils::ResultError(
                    "texture " + std::to_string(i) + " declares a size of " + std::to_string(texture.width) +
                    "x" + std::to_string(texture.height)
                );
            }

            // Pixels and the palette that follows them are read out of the file rather than
            // out of the cached block, because the loader uploads them before truncating.
            int64_t pixels = static_cast<int64_t>(texture.width) * texture.height;

            if (!Fits(texture.index, pixels + kPaletteBytes, 1, static_cast<int64_t>(size)))
            {
                return ncl_utils::ResultError(
                    Outside("texture " + std::to_string(i) + " pixel data",
                        texture.index, pixels + kPaletteBytes, static_cast<int64_t>(size))
                );
            }
        }

        return {};
    }
}
