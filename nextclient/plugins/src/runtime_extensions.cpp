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
        };
        std::mutex mutex;
        std::condition_variable wake;
        std::thread worker;
        bool stopping{};
        uint64_t next_result{}, next_request{};
        std::set<uint64_t> issued_tokens;
        std::map<uint64_t, Loaded*> owners;
        std::deque<Work> work, completed;
        std::deque<std::pair<uint64_t, std::string>> posts;
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
                completed.push_back(std::move(job));
            }
        }
        Json storage(Loaded& p, const std::string& operation, const Json& args)
        {
            if (operation != "commit" || p.store_pending)
                throw std::runtime_error("Unknown operation or commit pending");
            load_store(p);
            auto next = p.store;
            if (auto set = args.find("set"))
                for (const auto& [key, value] : set->get_object())
                {
                    if (!valid_id(key) || tao::json::to_string(value).size() > 65536)
                        throw std::runtime_error("Invalid store key or value");
                    next[key] = value;
                }
            if (auto remove = args.find("delete"))
                for (const auto& key : remove->get_array())
                    next.get_object().erase(key.get_string());
            if (next.get_object().size() > 1024 || tao::json::to_string(next).size() > 1024 * 1024)
                throw std::runtime_error("Store quota exceeded");
            const auto id = ++next_request;
            std::lock_guard lock(mutex);
            if (stopping || work.size() + completed.size() >= 256)
                throw std::runtime_error("Storage queue full or stopping");
            if (!worker.joinable())
                worker = std::thread(run_worker);
            work.push_back({p.token, id, store_path(p), std::move(next)});
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
                p->result_bytes -= it->second.size();
                p->results.erase(it);
            }
        }
        int32_t NC_CALL post(uint64_t token, const char* raw)
        {
            try
            {
                auto json = text(raw, 65536);
                parse(json);
                std::lock_guard lock(mutex);
                if (stopping || !owners.count(token) || posts.size() >= 1024 || post_bytes + json.size() > 1024 * 1024)
                    return 0;
                auto& usage = post_load[token];
                if (usage.first >= 64 || usage.second + json.size() > 128 * 1024)
                    return 0;
                posts.emplace_back(token, std::move(json));
                post_bytes += posts.back().second.size();
                ++usage.first;
                usage.second += posts.back().second.size();
                return 1;
            }
            catch (...)
            {
                return 0;
            }
        }
        template <int Interface>
        uint64_t NC_CALL call(void* ctx, const char* raw, const char* payload)
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p) || p->results.size() >= 64 || p->result_bytes > 512 * 1024)
                return 0;
            Json result;
            try
            {
                const auto operation = text(raw, 64);
                const auto args = parse(text(payload, 65536));
                if (!args.is_object())
                    throw std::runtime_error("Expected object");
                if constexpr (Interface == 0)
                {
                    if (operation != "stats")
                        throw std::runtime_error("Unknown operation");
                    result = Json{
                        {"ok", true},
                        {"queued", p->pending.size()},
                        {"dropped", p->dropped},
                        {"delivered", p->delivered},
                        {"callback_ms", p->callback_ms},
                        {"slow_callbacks", p->slow_callbacks}
                    };
                }
                if constexpr (Interface == 1)
                    result = storage(*p, operation, args);
                if constexpr (Interface == 2)
                {
                    if (operation != "token")
                        throw std::runtime_error("Unknown operation");
                    result = Json{{"ok", true}, {"token", p->token}};
                }
                if constexpr (Interface == 3)
                    result = extension_messages(*p, operation, args);
                if constexpr (Interface == 4)
                    result = extension_ui(*p, operation, args);
                if constexpr (Interface == 5)
                    result = extension_services(*p, operation, args);
                if constexpr (Interface == 6)
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
                        const auto bytes = file->Read(true);
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
                auto value = tao::json::to_string(result);
                const auto id = ++next_result;
                p->results.emplace(id, std::move(value));
                p->result_bytes += p->results.at(id).size();
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
        static const NcExtension interfaces[] = {
            {sizeof(NcExtension), 1, call<0>, read_result, release_result, nullptr, nullptr},
            {sizeof(NcExtension), 1, call<1>, read_result, release_result, nullptr, nullptr},
            {sizeof(NcExtension), 1, call<2>, read_result, release_result, post, nullptr},
            {sizeof(NcExtension), 1, call<3>, read_result, release_result, nullptr, set_message_filter},
            {sizeof(NcExtension), 1, call<4>, read_result, release_result, nullptr, nullptr},
            {sizeof(NcExtension), 1, call<5>, read_result, release_result, nullptr, nullptr},
            {sizeof(NcExtension), 1, call<6>, read_result, release_result, nullptr, nullptr}
        };
        static const char* names[] = {
            "nextclient.events",
            "nextclient.storage",
            "nextclient.tasks",
            "nextclient.messages",
            "nextclient.ui",
            "nextclient.services",
            "nextclient.package"
        };
        try
        {
            if (!available(static_cast<Loaded*>(ctx)) || version != 1)
                return nullptr;
            const auto name = text(raw, 64);
            for (size_t i = 0; i < std::size(names); ++i)
                if (name == names[i])
                    return &interfaces[i];
        }
        catch (...)
        {}
        return nullptr;
    }
    void extensions_attach(Loaded& p)
    {
        std::lock_guard lock(mutex);
        stopping = false;
        do
        {
            if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&p.token), sizeof(p.token), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
                throw std::runtime_error("Cannot create worker token");
        } while (!p.token || !issued_tokens.insert(p.token).second);
        owners.emplace(p.token, &p);
    }
    void services_detach(Loaded&);
    void services_pump();
    void extensions_detach(Loaded& p)
    {
        services_detach(p);
        std::lock_guard lock(mutex);
        owners.erase(p.token);
    }
    void extensions_pump()
    {
        std::deque<Work> done;
        std::deque<std::pair<uint64_t, std::string>> messages;
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
                    p.store = std::move(job.value);
                p.store_pending = false;
                notify(p, "sdk.storage", Json{{"request", job.id}, {"ok", job.success}});
            }
        for (auto& [owner, json] : messages)
            if (auto it = owners.find(owner); it != owners.end())
                notify(*it->second, "sdk.task", parse(json));
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
