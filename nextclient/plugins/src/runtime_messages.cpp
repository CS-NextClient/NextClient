#include "runtime_internal.h"
#include "runtime_schedule.h"
#include <cstring>

namespace plugins::runtime
{
    namespace
    {
        bool valid_message(const std::string& name)
        {
            return !name.empty() && name.size() <= 15 &&
                   name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string::npos;
        }
        uint32_t permission(const std::string& name)
        {
            return NC_PERMISSION_MESSAGES_READ | ((name == "SayText" || name == "TextMsg") ? NC_PERMISSION_CHAT_READ : 0);
        }
        bool presentation(const std::string& name)
        {
            return name == "ScreenFade" || name == "ScreenShake" || name == "SayText" || name == "TextMsg";
        }
        bool valid_payload(const std::string& name, const uint8_t* bytes, uint32_t size)
        {
            if (name == "ScreenFade")
                return size == 10;
            if (name == "ScreenShake")
                return size == 6;
            if (name == "SayText")
                return size >= 2 && size <= 192 && bytes[0] <= 32 && bytes[size - 1] == 0;
            if (name == "TextMsg")
                return size >= 2 && size <= 192 && bytes[0] >= 1 && bytes[0] <= 4 && bytes[size - 1] == 0;
            return false;
        }
    } // namespace
    Json extension_messages(Loaded& p, const std::string& operation, const Json& args)
    {
        const auto name = args.at("name").get_string();
        if (!valid_message(name) || !permitted(&p, permission(name)))
            throw std::runtime_error("Invalid message or permission denied");
        if (operation == "unsubscribe")
            p.messages.erase(name);
        else if (operation == "subscribe")
        {
            if (p.messages.size() >= 128)
                throw std::runtime_error("Subscription limit");
            if (!services.watch_message || !services.watch_message(name.c_str()))
                throw std::runtime_error("Message hook unavailable");
            p.messages.insert(name);
        }
        else
            throw std::runtime_error("Unknown operation");
        return Json{{"ok", true}};
    }
    int32_t NC_CALL set_message_filter(void* ctx, const char* raw, NcMessageFilter callback, void* user)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto name = text(raw, 15);
            if (!permitted(p, NC_PERMISSION_MESSAGES_FILTER | permission(name)) || !presentation(name))
                return 0;
            if (!callback)
            {
                p->filters.erase(name);
                return 1;
            }
            if (!services.watch_message || !services.watch_message(name.c_str()))
                return 0;
            p->filters[name] = {callback, user};
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

int32_t nc_runtime_message(
    const char* raw,
    const uint8_t* bytes,
    uint32_t size,
    double time,
    int32_t epoch,
    uint8_t* replacement,
    uint32_t* replacement_size
)
{
    if (GetCurrentThreadId() != main_thread || !started || size > 4096 || (!bytes && size) || !replacement || !replacement_size)
        return 0;
    try
    {
        const auto name = text(raw, 15);
        bool subscribed = false;
        for (const auto& p : loaded)
            subscribed |= available(p.get()) && (p->messages.count(name) || p->filters.count(name));
        if (!subscribed)
            return 0;
        Json data = tao::json::empty_array;
        for (uint32_t i = 0; i < size; ++i)
            data.push_back(bytes[i]);
        const Json event{{"name", name}, {"bytes", data}, {"time", time}, {"epoch", epoch}};
        for (auto& p : loaded)
            if (p->messages.count(name))
                notify(*p, "sdk.message", event, permission(name));
        std::vector<uint8_t> current;
        if (size)
            current.assign(bytes, bytes + size);
        bool changed = false, hidden = false;
        static bool filtering{};
        if (filtering)
            return 0;
        struct FilterGuard
        {
            bool& flag;
            ~FilterGuard()
            {
                flag = false;
            }
        } guard{filtering};
        filtering = true;
        // A filter can only change presentation. Observers always received the original bytes.
        static size_t cursor{};
        const auto preferred = preferred_callback(cursor, CallbackCategory::Filter, [&](const Loaded& p) {
            return p.filters.count(name) && permitted(&p, NC_PERMISSION_MESSAGES_FILTER | permission(name));
        });
        for (size_t index = 0; index < loaded.size(); ++index)
        {
            auto& p = loaded[index];
            if (auto it = p->filters.find(name); it != p->filters.end() &&
                                                 permitted(p.get(), NC_PERMISSION_MESSAGES_FILTER | permission(name)) &&
                                                 ordered_callback_budget(index, preferred))
            {
                uint8_t output[4096]{};
                uint32_t count = sizeof(output);
                const auto filter = it->second;
                int result = -1;
                try
                {
                    result = invoke(*p, CallbackCategory::Filter, [&] {
                        return filter.callback(
                            filter.user, name.c_str(), current.data(), static_cast<uint32_t>(current.size()), output, &count
                        );
                    });
                }
                catch (...)
                {}
                if (result < 0 || result > 2 || (result == 1 && (count > sizeof(output) || !valid_payload(name, output, count))))
                    fail(*p);
                else if (result == 1)
                {
                    current.assign(output, output + count);
                    changed = true;
                }
                else if (result == 2)
                    hidden = true;
            }
        }
        if (hidden)
            return 2;
        if (changed && *replacement_size >= current.size())
        {
            std::memcpy(replacement, current.data(), current.size());
            *replacement_size = static_cast<uint32_t>(current.size());
            return 1;
        }
    }
    catch (...)
    {}
    return 0;
}
