#include "runtime_internal.h"
#include <chrono>
#include <algorithm>

namespace plugins::runtime
{
    namespace
    {
        struct Service
        {
            Loaded* owner;
            uint32_t version, permission;
            bool topic;
            std::set<Loaded*> listeners;
            Budget memory;
        };
        struct Request
        {
            Loaded* caller;
            Loaded* provider;
            std::chrono::steady_clock::time_point deadline;
            std::string result;
            bool retain_completion;
            Budget memory;
        };
        std::map<std::string, Service> registry;
        std::map<uint64_t, Request> requests;
        uint64_t next_request{};
        bool authorized(const Loaded& caller, const Service& service)
        {
            if (!permitted(&caller, NC_PERMISSION_SERVICES_CALL | service.permission) || !available(service.owner))
                return false;
            if (&caller == service.owner)
                return true;
            for (const auto& dependency : caller.item.manifest.required)
                if (dependency.id == service.owner->item.manifest.id && matches(service.owner->item.manifest.version, dependency.range))
                    return true;
            return false;
        }
        void finish(uint64_t id, Request& request, const Json& value)
        {
            request.result = tao::json::to_string(value);
            request.provider = nullptr;
            notify(*request.caller, "sdk.reply", Json{{"request", id}, {"result", value}});
        }
    } // namespace
    std::string service_completion(Loaded& p, const std::string& operation, const Json& args)
    {
        const auto id = args.at("request").as<uint64_t>();
        auto it = requests.find(id);
        if (it == requests.end() || it->second.caller != &p)
            throw std::runtime_error("Unknown request");
        auto& request = it->second;
        if (operation == "cancel" && request.result.empty())
            finish(id, request, Json{{"ok", false}, {"error", "Request cancelled"}});
        // The stored result is already validated JSON. Do not rebuild a large
        // parsed tree just to retrieve it while normal resources are exhausted.
        auto result = "{\"ok\":true,\"request\":" + std::to_string(id);
        result += request.result.empty() ? ",\"state\":\"pending\"}"
                                         : ",\"state\":\"complete\",\"result\":" + request.result + "}";
        if (!request.result.empty() && !request.retain_completion)
            requests.erase(it);
        return result;
    }
    Json extension_services(Loaded& p, const std::string& operation, const Json& args)
    {
        if (operation == "ack")
        {
            const auto id = args.at("request").as<uint64_t>();
            auto it = requests.find(id);
            if (it == requests.end() || it->second.caller != &p)
                throw std::runtime_error("Unknown request");
            auto& request = it->second;
            if (request.result.empty())
                throw std::runtime_error("Request still pending");
            requests.erase(it);
            return Json{{"ok", true}};
        }
        if (operation == "register" || operation == "topic")
        {
            const auto local = args.at("name").get_string();
            if (!valid_id(local) || !p.registering || registry.size() >= 1024 ||
                std::count_if(registry.begin(), registry.end(), [&](const auto& entry) { return entry.second.owner == &p; }) >= 32)
                throw std::runtime_error("Invalid service registration or registration limit");
            const auto name = p.item.manifest.id + "/" + local;
            const auto version = static_cast<uint32_t>(integer(args.at("version"), 1, UINT32_MAX));
            const auto permission = args.find("permissions") ? permission_mask(args.at("permissions")) : 0;
            Budget memory(string_memory(name) + 1024);
            if (!registry.emplace(name, Service{&p, version, permission, operation == "topic", {}, std::move(memory)}).second)
                throw std::runtime_error("Service already registered");
            return Json{{"ok", true}, {"name", name}};
        }
        if (operation == "reply")
        {
            const auto id = args.at("request").as<uint64_t>();
            auto it = requests.find(id);
            if (it == requests.end() || it->second.provider != &p)
                throw std::runtime_error("Unknown request");
            finish(id, it->second, Json{{"ok", true}, {"data", args.at("data")}});
            if (!it->second.retain_completion)
                requests.erase(it);
            return Json{{"ok", true}};
        }
        const auto name = args.at("name").get_string();
        auto it = registry.find(name);
        if (it == registry.end() || !available(it->second.owner))
            throw std::runtime_error("Service unavailable");
        auto& service = it->second;
        if (operation == "publish")
        {
            if (!service.topic || service.owner != &p)
                throw std::runtime_error("Topic not owned");
            for (auto* listener : service.listeners)
                if (authorized(*listener, service))
                    notify(
                        *listener,
                        "sdk.topic",
                        Json{{"name", name}, {"version", service.version}, {"data", args.at("data")}},
                        NC_PERMISSION_SERVICES_CALL | service.permission
                    );
            return Json{{"ok", true}};
        }
        if (!authorized(p, service) || args.at("version").as<uint32_t>() != service.version)
            throw std::runtime_error("Dependency, permission or interface version mismatch");
        if (operation == "lookup")
            return Json{{"ok", true}, {"version", service.version}, {"topic", service.topic}};
        if (operation == "subscribe" || operation == "unsubscribe")
        {
            if (!service.topic)
                throw std::runtime_error("Not a topic");
            if (operation == "subscribe")
            {
                if (!service.listeners.count(&p))
                    service.memory.resize(service.memory.size() + 128);
                service.listeners.insert(&p);
            }
            else if (service.listeners.erase(&p))
                service.memory.resize(service.memory.size() - 128);
            return Json{{"ok", true}};
        }
        if (operation != "request" || service.topic || requests.size() >= 1024 ||
            std::count_if(requests.begin(), requests.end(), [&](const auto& entry) { return entry.second.caller == &p; }) >= 64)
            throw std::runtime_error("Invalid request or queue full");
        const auto method = args.at("method").get_string();
        if (!valid_id(method))
            throw std::runtime_error("Invalid method");
        const auto* retain = args.find("retain_completion");
        const bool retain_completion = retain && retain->get_boolean();
        const auto id = ++next_request;
        // Reserve both the terminal record and its bounded reply before acceptance.
        // Version-1 callers release automatically on completion. Only callers that
        // opt into durable completions retain an admission slot until acknowledgement.
        Budget memory(256 * 1024);
        auto data = tao::json::to_string(
            Json{
                {"name", name},
                {"version", service.version},
                {"request", id},
                {"caller", p.item.manifest.id},
                {"method", method},
                {"data", args.at("data")}
            }
        );
        requests.emplace(
            id,
            Request{
                &p,
                service.owner,
                std::chrono::steady_clock::now() + std::chrono::seconds(10),
                {},
                retain_completion,
                std::move(memory)
            }
        );
        if (!enqueue(*service.owner, {++event_serial, "sdk.service", std::move(data), {}, 0, true}))
        {
            requests.erase(id);
            throw std::runtime_error("Provider queue full");
        }
        return Json{{"ok", true}, {"request", id}};
    }
    void services_detach(Loaded& p)
    {
        for (auto it = requests.begin(); it != requests.end();)
            if (it->second.caller == &p)
            {
                it = requests.erase(it);
            }
            else
            {
                if (it->second.provider == &p)
                {
                    finish(it->first, it->second, Json{{"ok", false}, {"error", "Provider stopped"}});
                    if (!it->second.retain_completion)
                    {
                        it = requests.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        for (auto it = registry.begin(); it != registry.end();)
            if (it->second.owner == &p)
                it = registry.erase(it);
            else
            {
                if (it->second.listeners.erase(&p))
                    it->second.memory.resize(it->second.memory.size() - 128);
                ++it;
            }
    }
    void services_pump()
    {
        for (auto it = requests.begin(); it != requests.end();)
            if (it->second.result.empty() && it->second.deadline <= std::chrono::steady_clock::now())
            {
                finish(it->first, it->second, Json{{"ok", false}, {"error", "Request timed out"}});
                if (!it->second.retain_completion)
                    it = requests.erase(it);
                else
                    ++it;
            }
            else
                ++it;
    }
} // namespace plugins::runtime
