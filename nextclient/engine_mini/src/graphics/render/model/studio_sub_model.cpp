#include "graphics/render/model/studio_sub_model.h"

#include <cstdint>
#include <cstring>

namespace render
{
    const goldsrc::mstudiomodel_t* Studio_ResolveSubModel(const void* raw, const goldsrc::studiohdr_t* header)
    {
        if (!raw || !header || header->length < int(sizeof(*header)))
        {
            return nullptr;
        }

        const void* candidate = raw;

        for (int attempt = 0; attempt < 2; ++attempt)
        {
            uintptr_t offset = reinterpret_cast<uintptr_t>(candidate) - reinterpret_cast<uintptr_t>(header);
            uintptr_t length = uintptr_t(header->length);

            if (offset >= sizeof(*header) && offset <= length && sizeof(goldsrc::mstudiomodel_t) <= length - offset)
            {
                return static_cast<const goldsrc::mstudiomodel_t*>(candidate);
            }

            if (attempt == 0)
            {
                std::memcpy(&candidate, raw, sizeof(candidate));
            }
        }

        return nullptr;
    }
} // namespace render
