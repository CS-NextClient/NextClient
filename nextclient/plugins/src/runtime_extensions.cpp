#include "runtime_internal.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstring>
#include <bcrypt.h>

namespace plugins::runtime
{
    namespace
    {
        struct Work
        {
            uint64_t owner, id;
            fs::path path;
            Json value;
            bool success{};
            Budget memory;
        };
        std::mutex mutex;
        std::condition_variable wake;
        std::thread worker;
        bool stopping{};
        uint64_t next_result{}, next_request{};
        uint64_t token_counter{}, token_salt{};
        std::map<uint64_t, Loaded*> owners;
        std::deque<Work> work, completed;
        struct Post
        {
            uint64_t owner;
            std::string json;
            Budget memory;
        };
        std::deque<Post> posts;
        size_t post_bytes{};
        std::map<uint64_t, std::pair<size_t, size_t>> post_load;

        void run_worker()
        {
            for (;;)
            {
                Work job;
                {
                    std::unique_lock lock(mutex);
                    wake.wait(lock, [] { return stopping || !work.empty(); });
                    if (work.empty())
                        return;
                    job = std::move(work.front());
                    work.pop_front();
                }
                try
                {
                    write_json(job.path, job.value);
                    job.success = true;
                }
                catch (...)
                {}
                std::lock_guard lock(mutex);
                if (owners.count(job.owner))
                    completed.push_back(std::move(job));
            }
        }
        Json storage(Loaded& p, const std::string& operation, const Json& args)
        {
            if (operation != "commit" || p.store_pending)
                throw std::runtime_error("Unknown operation or commit pending");
            const auto* set = args.find("set");
            const auto* remove = args.find("delete");
            auto candidate = prepare_store(p, set ? *set : Json(tao::json::empty_object), remove ? *remove : Json(tao::json::empty_array));
            const auto id = ++next_request;
            std::lock_guard lock(mutex);
            if (stopping || work.size() + completed.size() >= 256)
                throw std::runtime_error("Storage queue full or stopping");
            if (!worker.joinable())
                worker = std::thread(run_worker);
            work.push_back({p.token, id, store_path(p), std::move(candidate.value), false, std::move(candidate.memory)});
            p.store_pending = true;
            wake.notify_one();
            return Json{{"ok", true}, {"request", id}};
        }
        uint32_t NC_CALL read_result(void* ctx, uint64_t id, char* out, uint32_t capacity)
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p))
                return 0;
            auto it = p->results.find(id);
            if (it == p->results.end())
                return 0;
            const auto size = static_cast<uint32_t>(it->second.size() + 1);
            if (out && capacity >= size)
                std::memcpy(out, it->second.c_str(), size);
            return size;
        }
        void NC_CALL release_result(void* ctx, uint64_t id)
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p))
                return;
            auto it = p->results.find(id);
            if (it != p->results.end())
            {
                p->result_memory.resize(p->result_memory.size() - string_memory(it->second) - 128);
                p->results.erase(it);
            }
        }
        int32_t NC_CALL post(uint64_t token, const char* raw)
        {
            try
            {
                std::lock_guard lock(mutex);
                if (stopping || !owners.count(token))
                    return 0;
                // A payload-free probe is a worker-safe cooperative cancellation check.
                if (!raw)
                    return 1;
                auto json = text(raw, 65536);
                parse(json);
                if (posts.size() >= 1024 || post_bytes + json.size() > 1024 * 1024)
                    return 0;
                auto& usage = post_load[token];
                if (usage.first >= 64 || usage.second + json.size() > 128 * 1024)
                    return 0;
                Budget memory(string_memory(json) + 256);
                posts.push_back({token, std::move(json), std::move(memory)});
                post_bytes += posts.back().json.size();
                ++usage.first;
                usage.second += posts.back().json.size();
                return 1;
            }
            catch (...)
            {
                return 0;
            }
        }
        enum class Interface
        {
            Events,
            Storage,
            Tasks,
            Messages,
            UI,
            Services,
            Package
        };
        template <Interface Kind>
        uint64_t NC_CALL call(void* ctx, const char* raw, const char* payload)
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p) || p->results.size() >= 64 || p->result_memory.size() > 1024 * 1024)
                return 0;
            Budget response;
            try
            {
                response.resize(1024 * 1024, true);
            }
            catch (...)
            {
                return 0;
            } // Reject before executing a side effect.
            Json result;
            std::string serialized;
            try
            {
                const auto operation = text(raw, 64);
                const auto args = parse(text(payload, 65536));
                Budget arguments(json_memory(args), true);
                if (!args.is_object())
                    throw std::runtime_error("Expected object");
                if constexpr (Kind == Interface::Events)
                {
                    if (operation != "stats")
                        throw std::runtime_error("Unknown operation");
                    result = resource_stats(p);
                }
                if constexpr (Kind == Interface::Storage)
                    result = storage(*p, operation, args);
                if constexpr (Kind == Interface::Tasks)
                {
                    if (operation != "token")
                        throw std::runtime_error("Unknown operation");
                    result = Json{{"ok", true}, {"token", p->token}};
                }
                if constexpr (Kind == Interface::Messages)
                    result = extension_messages(*p, operation, args);
                if constexpr (Kind == Interface::UI)
                    result = extension_ui(*p, operation, args);
                if constexpr (Kind == Interface::Services)
                {
                    if (operation == "status" || operation == "cancel")
                        serialized = service_completion(*p, operation, args);
                    else
                        result = extension_services(*p, operation, args);
                }
                if constexpr (Kind == Interface::Package)
                {
                    if (operation != "read" || !p->package)
                        throw std::runtime_error("Unknown operation");
                    const auto name = args.at("path").get_string();
                    if (name.empty() || name.size() > 256 || name.find('\\') != std::string::npos || name.find(':') != std::string::npos)
                        throw std::runtime_error("Invalid asset path");
                    const auto directory = fs::path(p->package->files.at(p->package->entry)->ResolvedPath()).parent_path();
                    bool found{};
                    for (const auto& file : p->package->files)
                    {
                        const auto relative = fs::path(file->ResolvedPath()).lexically_relative(directory).generic_u8string();
                        if (std::string(relative.begin(), relative.end()) != name)
                            continue;
                        const auto bytes = file->Read(true, 65536);
                        if (bytes.size() > 65536)
                            throw std::runtime_error("Asset exceeds 64 KiB read limit");
                        Json data = tao::json::empty_array;
                        for (auto byte : bytes)
                            data.push_back(byte);
                        result = Json{{"ok", true}, {"bytes", std::move(data)}};
                        found = true;
                        break;
                    }
                    if (!found)
                        throw std::runtime_error("Asset is not part of the approved package");
                }
            }
            catch (const std::exception& e)
            {
                result = Json{{"ok", false}, {"error", e.what()}};
            }
            catch (...)
            {
                return 0;
            }
            try
            {
                auto value = serialized.empty() ? tao::json::to_string(result) : std::move(serialized);
                response.resize(string_memory(value) + 128);
                const auto id = ++next_result;
                p->results.emplace(id, std::move(value));
                p->result_memory.absorb(std::move(response));
                return id;
            }
            catch (...)
            {
                return 0;
            }
        }
    } // namespace
    int32_t NC_CALL set_message_filter(void*, const char*, NcMessageFilter, void*);
    const NcExtension* NC_CALL query_interface(void* ctx, const char* raw, uint32_t version)
    {
        struct Descriptor
        {
            const char* name;
            NcExtension api;
        };
        static const Descriptor interfaces[]{
            {"nextclient.events", {sizeof(NcExtension), 1, call<Interface::Events>, read_result, release_result, nullptr, nullptr}},
            {"nextclient.storage", {sizeof(NcExtension), 1, call<Interface::Storage>, read_result, release_result, nullptr, nullptr}},
            {"nextclient.tasks", {sizeof(NcExtension), 1, call<Interface::Tasks>, read_result, release_result, post, nullptr}},
            {"nextclient.messages",
             {sizeof(NcExtension), 1, call<Interface::Messages>, read_result, release_result, nullptr, set_message_filter}},
            {"nextclient.ui", {sizeof(NcExtension), 1, call<Interface::UI>, read_result, release_result, nullptr, nullptr}},
            {"nextclient.services", {sizeof(NcExtension), 1, call<Interface::Services>, read_result, release_result, nullptr, nullptr}},
            {"nextclient.package", {sizeof(NcExtension), 1, call<Interface::Package>, read_result, release_result, nullptr, nullptr}},
        };
        try
        {
            if (!available(static_cast<Loaded*>(ctx)) || version != 1)
                return nullptr;
            const auto name = text(raw, 64);
            for (const auto& descriptor : interfaces)
                if (name == descriptor.name)
                    return &descriptor.api;
        }
        catch (...)
        {}
        return nullptr;
    }
    void extensions_attach(Loaded& p)
    {
        std::lock_guard lock(mutex);
        stopping = false;
        if (!token_counter)
        {
            if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&token_salt), sizeof(token_salt), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
                throw std::runtime_error("Cannot create worker token");
        }
        do
        {
            if (token_counter == UINT64_MAX)
                throw std::runtime_error("Worker token space exhausted");
            p.token = ++token_counter ^ token_salt;
        } while (!p.token);
        owners.emplace(p.token, &p);
    }
    void services_detach(Loaded&);
    void services_pump();
    void extensions_detach(Loaded& p)
    {
        services_detach(p);
        std::lock_guard lock(mutex);
        owners.erase(p.token);
        std::erase_if(work, [&](const auto& job) { return job.owner == p.token; });
        std::erase_if(completed, [&](const auto& job) { return job.owner == p.token; });
        std::erase_if(posts, [&](const auto& post) {
            if (post.owner != p.token)
                return false;
            post_bytes -= post.json.size();
            return true;
        });
        post_load.erase(p.token);
    }
    void extensions_pump()
    {
        std::deque<Work> done;
        std::deque<Post> messages;
        {
            std::lock_guard lock(mutex);
            done.swap(completed);
            messages.swap(posts);
            post_bytes = 0;
            post_load.clear();
        }
        // Only this thread reads owner pointers; other threads only test tokens.
        for (auto& job : done)
            if (auto it = owners.find(job.owner); it != owners.end())
            {
                auto& p = *it->second;
                if (job.success)
                {
                    p.store = std::move(job.value);
                    p.store_memory = std::move(job.memory);
                }
                p.store_pending = false;
                notify(p, "sdk.storage", Json{{"request", job.id}, {"ok", job.success}});
            }
        for (auto& post : messages)
            if (auto it = owners.find(post.owner); it != owners.end())
                enqueue(*it->second, {++event_serial, "sdk.task", std::move(post.json), {}, 0, true});
        services_pump();
    }
    void extensions_stop()
    {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        wake.notify_one();
        if (worker.joinable())
            worker.join();
        extensions_pump();
    }
} // namespace plugins::runtime
