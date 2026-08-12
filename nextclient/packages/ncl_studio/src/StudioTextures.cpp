#include "ncl_studio/StudioTextures.h"

#include <string>

// studio.h uses vec3_t without declaring it.
#include <tier0/basetypes.h>
#include <studio.h>

#include "HeaderShape.h"

namespace ncl_studio
{
    namespace
    {
        // The count is taken wider than the file's own int, because a table whose size is a
        // product of two of them can be a number that does not fit one.
        bool Fits(size_t size, int offset, int64_t count, size_t element_size)
        {
            if (offset < 0 || count < 0)
            {
                return false;
            }

            size_t start = static_cast<size_t>(offset);
            if (start > size)
            {
                return false;
            }

            size_t available = size - start;

            return element_size == 0 || static_cast<uint64_t>(count) <= available / element_size;
        }
    }

    int32_t SkinTable::TextureFor(int32_t family, int32_t reference) const
    {
        if (family < 0 || family >= families || reference < 0 || reference >= references)
        {
            return -1;
        }

        return values[static_cast<size_t>(family) * references + reference];
    }

    ncl_utils::ResultT<StudioTextures> ReadStudioTextures(const void* data, size_t size)
    {
        if (data == nullptr || size < sizeof(studiohdr_t))
        {
            return ncl_utils::ResultError("file is smaller than a studio header");
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        const studiohdr_t* header = reinterpret_cast<const studiohdr_t*>(bytes);

        std::string shape = HeaderShapeError(header);
        if (!shape.empty())
        {
            return ncl_utils::ResultError(shape);
        }

        if (!Fits(size, header->textureindex, header->numtextures, sizeof(mstudiotexture_t)))
        {
            return ncl_utils::ResultError("texture table lies outside the file");
        }

        StudioTextures result;

        const mstudiotexture_t* textures = reinterpret_cast<const mstudiotexture_t*>(bytes + header->textureindex);

        for (int i = 0; i < header->numtextures; i++)
        {
            TextureInfo info;

            // The name is a fixed array in the file and is not required to be terminated,
            // so it is bounded here rather than handed to a string constructor.
            const char* name = textures[i].name;
            size_t length = 0;
            while (length < sizeof(textures[i].name) && name[length] != '\0')
            {
                length++;
            }

            info.name.assign(name, length);
            info.flags = textures[i].flags;
            info.width = textures[i].width;
            info.height = textures[i].height;

            if (info.width <= 0 || info.height <= 0)
            {
                return ncl_utils::ResultError("texture \"" + info.name + "\" declares no size");
            }

            result.textures.push_back(std::move(info));
        }

        if (header->numskinfamilies < 0 || header->numskinref < 0)
        {
            return ncl_utils::ResultError("skin table declares a negative size");
        }

        // Widened before it is multiplied: the two dimensions come out of the file, and
        // their product in the file's own int wraps to a small number that passes the bound
        // it does not meet. The table would then be shorter than the dimensions say, and
        // every reader of it indexes by those dimensions.
        int64_t skin_values = static_cast<int64_t>(header->numskinfamilies) * header->numskinref;

        if (!Fits(size, header->skinindex, skin_values, sizeof(short)))
        {
            return ncl_utils::ResultError("skin table lies outside the file");
        }

        result.skins.families = header->numskinfamilies;
        result.skins.references = header->numskinref;

        const short* skins = reinterpret_cast<const short*>(bytes + header->skinindex);
        size_t count = static_cast<size_t>(skin_values);

        for (size_t i = 0; i < count; i++)
        {
            // A reference pointing at no texture would send a caller reading the table off
            // the end of it, and the file is not trusted to be consistent with itself.
            if (skins[i] < 0 || skins[i] >= header->numtextures)
            {
                return ncl_utils::ResultError("skin table names a texture the model does not have");
            }

            result.skins.values.push_back(skins[i]);
        }

        return std::move(result);
    }
}
