#include "runtime_internal.h"

namespace plugins::runtime
{
    StoreCandidate prepare_store(Loaded& p, const Json& set, const Json& remove)
    {
        if (p.store_pending)
            throw std::runtime_error("Store commit pending");
        load_store(p);
        Budget memory(json_memory(p.store));
        auto next = p.store;
        for (const auto& [key, value] : set.get_object())
        {
            if (!valid_id(key) || tao::json::to_string(value).size() > 65536)
                throw std::runtime_error("Invalid store key or value");
            next[key] = value;
        }
        for (const auto& key : remove.get_array())
        {
            if (!valid_id(key.get_string()))
                throw std::runtime_error("Invalid store key");
            next.get_object().erase(key.get_string());
        }
        if (next.get_object().size() > 1024)
            throw std::runtime_error("Store quota exceeded");
        // Validate size and depth including the outer store before either path
        // writes it. The committed value must remain readable after restart.
        parse(tao::json::to_string(next));
        memory.resize(json_memory(next) + 2048);
        return {std::move(next), std::move(memory)};
    }
} // namespace plugins::runtime
