#include "runtime_internal.h"
#include "plugin_limits.h"
#include <shellapi.h>
#include <cstring>

namespace plugins::runtime
{
    fs::path root;
    Json profile = tao::json::empty_object, settings = tao::json::empty_object;
    std::vector<std::unique_ptr<Loaded>> loaded;
    std::vector<Item> observed;
    bool started = false, safe = false;
    std::string startup_error;
    DWORD main_thread{};
    NcClientServices services{};
    Loaded* drawing{};
    uint64_t event_serial{};
    bool dispatching_events{};
    std::vector<DrawOperation> draw_operations;
    size_t draw_text_bytes{};
    bool available(const Loaded* p)
    {
        return GetCurrentThreadId() == main_thread && p && !p->failed;
    }
    bool permitted(const Loaded* p, uint32_t permission)
    {
        return available(p) && (p->item.manifest.permissions & permission) == permission;
    }

    std::string text(const char* s, size_t limit)
    {
        if (!s)
            return {};
        size_t n = strnlen_s(s, limit + 1);
        if (n > limit)
            throw std::runtime_error(message("#NextPlugins_ErrorSdkString"));
        return {s, n};
    }
    bool valid_id(const std::string& s)
    {
        return !s.empty() && s.size() <= max_id_length &&
               s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") == std::string::npos;
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

void nc_runtime_start(const wchar_t* directory, int safe_mode)
{
    if (started)
        return;
    started = true;
    main_thread = GetCurrentThreadId();
    startup_error.clear();
    try
    {
        if (directory)
            root = fs::absolute(directory);
        else
        {
            wchar_t path[32768]{};
            if (!GetModuleFileNameW(nullptr, path, 32768))
                throw std::runtime_error(message("#NextPlugins_ErrorGameDirectory"));
            root = fs::path(path).parent_path();
        }
        safe = safe_mode > 0;
        if (safe_mode < 0)
        {
            int count{};
            auto args = CommandLineToArgvW(GetCommandLineW(), &count);
            if (args)
            {
                for (int n = 1; n < count; ++n)
                    if (!_wcsicmp(args[n], L"-noplugins"))
                        safe = true;
                LocalFree(args);
            }
        }
        profile = parse(read_text(root / L"plugins" / L"profile.json"));
        settings = parse(read_text(root / L"plugins" / L"settings.json"));
        if (!profile.is_object() || !settings.is_object())
            throw std::runtime_error(message("#NextPlugins_ErrorConfig"));
        discover();
        if (safe)
            return;
        for (auto& a : observed)
            if (a.enabled)
                for (auto& b : observed)
                    if (b.enabled)
                        for (const auto& r : a.manifest.conflicts)
                            if (r.id == b.manifest.id && matches(b.manifest.version, r.range))
                            {
                                a.error = message("#NextPlugins_ErrorConflict", {display_name(b), r.reason});
                                b.error = message("#NextPlugins_ErrorConflict", {display_name(a), r.reason});
                            }
        // Walk user order once. A failed dependency never allows its dependents
        // to load, while unrelated plugins remain usable (including cycles).
        std::set<std::string> success;
        for (auto item : observed)
        {
            if (!item.enabled || !item.approved || !item.error.empty())
                continue;
            bool ready = true;
            for (const auto& r : item.manifest.required)
                if (!success.count(r.id))
                    ready = false;
            auto isolated = observed;
            for (auto& i : isolated)
                i.enabled = i.enabled && (i.file == item.file || success.count(i.manifest.id));
            if (!ready || !validate(isolated).empty())
                continue;
            auto p = std::make_unique<Loaded>();
            p->item = item;
            try
            {
                p->package = read_package(root / L"plugins" / fs::path(std::u8string(item.file.begin(), item.file.end())));
                if (p->package->hash != item.hash)
                    throw std::runtime_error(message("#NextPlugins_ErrorApprovalChanged"));
                // Dependencies are resolved only from System32 or modules the
                // game already loaded. Never search an unapproved plugin folder.
                p->module = p->package->Load();
                if (!p->module)
                    throw std::runtime_error(message("#NextPlugins_ErrorLoadDll"));
                auto entry = reinterpret_cast<NcEntry>(GetProcAddress(p->module, "nc_plugin_entry"));
                if (!entry)
                    throw std::runtime_error(message("#NextPlugins_ErrorEntryMissing"));
                const auto* api = entry();
                if (!api || api->size < sizeof(NcPlugin) || api->abi != NC_ABI_VERSION || api->api != NC_API_VERSION || !api->load)
                    throw std::runtime_error(message("#NextPlugins_ErrorEntryMismatch"));
                p->api = *api;
                extensions_attach(*p);
                p->host = make_host(*p);
                if (p->api.load(&p->host) != 0)
                    throw std::runtime_error(message("#NextPlugins_ErrorInitialization"));
                p->registering = false;
                if (!install_commands(*p))
                    throw std::runtime_error(message("#NextPlugins_ErrorConsoleRegistration"));
                success.insert(item.manifest.id);
                loaded.push_back(std::move(p));
            }
            catch (const std::exception& e)
            {
                for (auto& i : observed)
                    if (i.file == item.file)
                        i.error = error_message(e);
                if (p)
                    p->failed = true;
                if (p && p->api.unload)
                    p->api.unload();
                if (p)
                    extensions_detach(*p);
            }
        }
        // Preserve startup diagnostics across refreshes below.
        for (const auto& i : observed)
            if (i.enabled && !success.count(i.manifest.id))
                append_message(
                    startup_error,
                    message(
                        "#NextPlugins_DiagnosticContext",
                        {display_name(i), i.error.empty() ? message_value("#NextPlugins_ErrorBlocked") : parse(i.error)}
                    )
                );
    }
    catch (const std::exception& e)
    {
        startup_error = error_message(e);
        safe = true;
    }
}
void nc_runtime_stop()
{
    extensions_stop();
    for (auto it = loaded.rbegin(); it != loaded.rend(); ++it)
    {
        auto& p = **it;
        p.failed = true;
        if (p.api.unload)
            p.api.unload();
        extensions_detach(p);
    }
    loaded.clear();
    observed.clear();
    profile = tao::json::empty_object;
    settings = tao::json::empty_object;
    started = false;
}
