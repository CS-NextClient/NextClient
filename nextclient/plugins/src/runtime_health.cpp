#include "runtime_internal.h"
#include <algorithm>
#include <cstring>

namespace plugins::runtime
{
    double frame_callback_ms{};
    uint64_t deferred_callbacks{};
    std::array<bool, static_cast<size_t>(CallbackCategory::Count)> callback_categories{};
    namespace
    {
        HANDLE trace_file = INVALID_HANDLE_VALUE, trace_mapping{};
        char* trace{};
        constexpr size_t slot_size = 1024, slots = 65;
        size_t trace_serial{};
        std::string active;
        Json session = tao::json::empty_object;
        bool marked{}, pending_recovery{};
        fs::path path(const wchar_t* name)
        {
            return root / L"plugins" / name;
        }
        void trace_line(size_t slot, const std::string& value)
        {
            if (!trace)
                return;
            auto* line = trace + slot * slot_size;
            std::memset(line, ' ', slot_size - 1);
            std::memcpy(line, value.data(), std::min(value.size(), slot_size - 1));
            line[slot_size - 1] = '\n';
        }
        void close_trace()
        {
            if (trace)
            {
                FlushViewOfFile(trace, 0);
                UnmapViewOfFile(trace);
                trace = nullptr;
            }
            if (trace_mapping)
            {
                CloseHandle(trace_mapping);
                trace_mapping = nullptr;
            }
            if (trace_file != INVALID_HANDLE_VALUE)
            {
                CloseHandle(trace_file);
                trace_file = INVALID_HANDLE_VALUE;
            }
            active.clear();
        }
    } // namespace
    bool callback_budget(CallbackCategory category)
    {
        return frame_callback_ms < 6.0 || !callback_categories[static_cast<size_t>(category)];
    }
    void reset_callback_budget()
    {
        frame_callback_ms = 0;
        callback_categories.fill(false);
    }
    CallbackScope::CallbackScope(Loaded& p, const char* category) noexcept :
        plugin_(p),
        category_(category),
        previous_(std::move(active))
    {
        try
        {
            active = tao::json::to_string(
                Json{
                    {"id", p.item.manifest.id},
                    {"hash", p.item.hash},
                    {"category", category},
                    {"active", true},
                    {"sequence", ++trace_serial}
                }
            );
            trace_line(0, active);
            trace_line(1 + trace_serial % (slots - 1), active);
        }
        catch (...)
        {}
        begin_ = std::chrono::steady_clock::now();
    }
    CallbackScope::~CallbackScope()
    {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin_).count();
        auto& timing = plugin_.timings.find(category_)->second;
        ++timing.count;
        timing.last_ms = plugin_.callback_ms = ms;
        timing.total_ms += ms;
        timing.max_ms = std::max(timing.max_ms, ms);
        if (ms > 2)
        {
            ++timing.slow;
            ++plugin_.slow_callbacks;
        }
        const std::string_view category(category_);
        constexpr const char* names[] = {"event", "frame", "draw", "command", "filter"};
        for (size_t i = 0; i < callback_categories.size(); ++i)
            if (category == names[i])
            {
                callback_categories[i] = true;
                frame_callback_ms += ms;
                break;
            }
        try
        {
            trace_line(
                1 + (++trace_serial) % (slots - 1),
                tao::json::to_string(
                    Json{
                        {"id", plugin_.item.manifest.id}, {"category", category_}, {"active", false}, {"ms", ms}, {"sequence", trace_serial}
                    }
                )
            );
            active = std::move(previous_);
            trace_line(0, active.empty() ? "{\"active\":false}" : active);
        }
        catch (...)
        {} // Diagnostics must never turn a completed callback into a failure.
    }
    Json resource_stats(const Loaded* p)
    {
        Json value{
            {"ok", true},
            {"host_memory_estimate", memory_used.load()},
            {"host_memory_limit", memory_limit},
            {"host_memory_peak", memory_peak.load()},
            {"memory_rejections", memory_denied.load()},
            {"frame_callback_ms", frame_callback_ms},
            {"frame_budget_ms", 6},
            {"deferred_callbacks", deferred_callbacks}
        };
        if (p)
        {
            value["queued"] = p->pending.size();
            value["queued_json_bytes"] = p->pending_bytes;
            value["dropped"] = p->dropped;
            value["delivered"] = p->delivered;
            value["callback_ms"] = p->callback_ms;
            value["slow_callbacks"] = p->slow_callbacks;
            Json categories = tao::json::empty_object;
            for (const auto& [name, t] : p->timings)
                categories[name] =
                    Json{{"count", t.count}, {"slow", t.slow}, {"last_ms", t.last_ms}, {"max_ms", t.max_ms}, {"total_ms", t.total_ms}};
            value["callbacks"] = std::move(categories);
        }
        return value;
    }
    void recovery_start(bool interactive)
    {
        close_trace();
        marked = false;
        pending_recovery = fs::exists(path(L"session.json"));
        reset_callback_budget();
        if (pending_recovery)
        {
            fs::copy_file(path(L"session.json"), path(L"session.previous.json"), fs::copy_options::overwrite_existing);
            if (fs::exists(path(L"session-trace.txt")))
                fs::copy_file(path(L"session-trace.txt"), path(L"session-trace.previous.txt"), fs::copy_options::overwrite_existing);
            append_message(startup_error, message("#NextPlugins_UncleanSession"));
            try
            {
                auto prior = parse(read_text(path(L"session.json")));
                std::string suspect;
                if (prior.find("phase") && prior.at("phase") == "initializing" && prior.find("suspected"))
                    suspect = prior.at("suspected").get_string();
                if (fs::exists(path(L"session-trace.txt")))
                {
                    const auto lines = read_text(path(L"session-trace.txt"));
                    const auto callback = parse(lines.substr(0, lines.find('\n')));
                    if (callback.find("active") && callback.at("active") == true && callback.find("id"))
                        suspect = callback.at("id").get_string();
                }
                if (!suspect.empty())
                    append_message(startup_error, message("#NextPlugins_Suspected", {suspect}));
            }
            catch (...)
            {}
            if (!safe)
            {
                const bool russian = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
                safe =
                    !interactive ||
                    MessageBoxW(
                        nullptr,
                        russian ? L"Предыдущий сеанс завершился некорректно. Возможно участие плагина, но причина не доказана.\n\n"
                                  L"Запустить без плагинов? (Рекомендуется)\n\nВ разделе «Настройки > Плагины» можно отключить "
                                  L"подозрительный плагин и нажать OK."
                                : L"The previous session did not close cleanly. A plugin may be involved, but this is not proof of the "
                                  L"cause.\n\n"
                                  L"Start without plugins? (Recommended)\n\nIn Options > Plugins, disable a suspected plugin and click OK.",
                        russian ? L"Восстановление плагинов NextClient" : L"NextClient plugin recovery",
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1
                    ) != IDNO;
            }
        }
    }
    bool recovery_pending()
    {
        return pending_recovery;
    }
    void recovery_acknowledge()
    {
        // Never remove a marker protecting modules active in this session.
        if (!pending_recovery)
            return;
        if (!marked)
            fs::remove(path(L"session.json"));
        pending_recovery = false;
    }
    static void begin_session()
    {
        fs::create_directories(path(L".host") / L"settings");
        session = Json{{"schema", 1}, {"phase", "starting"}, {"suspected", ""}, {"plugins", tao::json::empty_array}};
        write_json(path(L"session.json"), session);
        marked = true;
        trace_file = CreateFileW(
            path(L"session-trace.txt").c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (trace_file == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create plugin trace");
        trace_mapping = CreateFileMappingW(trace_file, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(slots * slot_size), nullptr);
        if (!trace_mapping)
            throw std::runtime_error("Cannot map plugin trace");
        trace = static_cast<char*>(MapViewOfFile(trace_mapping, FILE_MAP_WRITE, 0, 0, 0));
        if (!trace)
            throw std::runtime_error("Cannot open plugin trace");
        trace_serial = 0;
        for (size_t i = 0; i < slots; ++i)
            trace_line(i, "{}");
    }
    void recovery_loading(const Loaded& p)
    {
        // An empty/disabled installation never executes plugin code and needs no
        // crash marker. Create it immediately before the first native load.
        if (!marked)
            begin_session();
        session["phase"] = "initializing";
        session["suspected"] = p.item.manifest.id;
        session["plugins"].push_back(
            Json{
                {"id", p.item.manifest.id},
                {"version", p.item.manifest.version},
                {"hash", p.item.hash},
                {"file", p.item.file},
                {"order", session.at("plugins").get_array().size()}
            }
        );
        write_json(path(L"session.json"), session);
    }
    void recovery_running()
    {
        if (!marked)
            return;
        session["phase"] = "running";
        session["suspected"] = "";
        write_json(path(L"session.json"), session);
    }
    void recovery_finish()
    {
        close_trace();
        if (!marked)
            return;
        try
        {
            fs::remove(path(L"session.json"));
        }
        catch (...)
        {
            // Failure to remove the marker must not interrupt shutdown.
        }
        session = tao::json::empty_object;
        marked = false;
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

const char* nc_runtime_stats()
{
    static std::string output;
    Json list = tao::json::empty_array;
    for (const auto& p : loaded)
        list.push_back(Json{{"file", p->item.file}, {"hash", p->item.hash}, {"resources", resource_stats(p.get())}});
    output = tao::json::to_string(Json{{"resources", resource_stats()}, {"plugins", std::move(list)}});
    return output.c_str();
}

int32_t nc_runtime_recovery_pending()
{
    return recovery_pending() ? 1 : 0;
}

const char* nc_runtime_acknowledge_recovery()
{
    static std::string error;
    error.clear();
    try
    {
        recovery_acknowledge();
    }
    catch (const std::exception& e)
    {
        error = error_message(e);
    }
    return error.c_str();
}
