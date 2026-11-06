#pragma once

#include <cstddef>
#include <cstdint>

namespace ncl_studio
{
    inline bool TableFits(size_t size, int offset, int64_t count, size_t element_size)
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

        return element_size == 0 || static_cast<uint64_t>(count) <= (size - start) / element_size;
    }
} // namespace ncl_studio
