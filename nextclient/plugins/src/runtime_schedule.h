#pragma once
#include "runtime_internal.h"

namespace plugins::runtime
{
    // Pick the owner entitled to this category's one progress exception before
    // traversing an order-sensitive chain. Earlier callbacks cannot consume it.
    template <class Eligible>
    size_t preferred_callback(size_t& cursor, CallbackCategory category, Eligible&& eligible)
    {
        if (!callback_categories[static_cast<size_t>(category)])
            for (size_t count = 0; count < loaded.size(); ++count)
            {
                const auto index = (cursor + count) % loaded.size();
                if (eligible(*loaded[index]))
                {
                    cursor = index + 1;
                    return index;
                }
            }
        return loaded.size();
    }

    inline bool ordered_callback_budget(size_t index, size_t preferred)
    {
        if (index == preferred || frame_callback_ms < 6.0)
            return true;
        ++deferred_callbacks;
        return false;
    }

    inline bool pass_budget(CallbackCategory category, std::chrono::steady_clock::time_point start)
    {
        return callback_budget(category) && (!callback_categories[static_cast<size_t>(category)] ||
                                             std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2));
    }
} // namespace plugins::runtime
