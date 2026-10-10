#pragma once
#include "catalog.h"
#include "plugin_file.h"
#include "plugin_limits.h"
#include "plugin_package.h"
#include "runtime_budget.h"
#include "runtime_callbacks.h"
#include <nextclient/runtime.h>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <chrono>
#include <array>

namespace plugins::runtime
{
    namespace fs = std::filesystem;
    struct Event
    {
        uint64_t serial{};
        std::string name, json, cvar;
        uint32_t permission{};
        bool direct{};
        uint64_t service_request{};
        Budget memory;
    };
    struct Loaded
    {
        Item item;
        std::unique_ptr<Package> package;
        HMODULE module{};
        NcPlugin api{};
        NcHost host{};
        Json tabs = tao::json::empty_array, controls = tao::json::empty_array;
        Json settings = tao::json::empty_array;
        Json values = tao::json::empty_object;
        std::vector<std::string> commands;
        uint32_t hidden_ui{};
        std::map<std::string, uint64_t> events, cvar_watches;
        struct Cvar
        {
            std::string name, initial;
            int32_t archive;
        };
        std::vector<Cvar> cvars;
        Json store = tao::json::empty_object;
        bool store_loaded{};
        bool store_pending{};
        uint64_t token{}, dropped{}, first_dropped{}, last_dropped{}, delivered{}, slow_callbacks{};
        std::deque<Event> pending;
        size_t pending_bytes{};
        double callback_ms{};
        std::map<uint64_t, std::string> results;
        std::set<std::string> messages;
        struct Filter
        {
            NcMessageFilter callback{};
            void* user{};
        };
        std::map<std::string, Filter> filters;
        Json windows = tao::json::empty_object;
        uint64_t ui_revision{};
        bool registering = true, failed = false;
        bool retired{};
        Budget base_memory, values_memory, store_memory, ui_memory, registration_memory, result_memory;
        struct Timing
        {
            uint64_t count{}, slow{};
            double total_ms{}, max_ms{}, last_ms{};
        };
        std::array<Timing, callback_count> timings{};
    };
    struct DrawOperation
    {
        int32_t x, y, width, height;
        uint32_t rgba;
        bool is_text{};
        std::string text;
    };

    extern fs::path root;
    extern Json profile;
    extern std::vector<std::unique_ptr<Loaded>> loaded;
    extern std::vector<Item> observed;
    extern Budget catalog_memory;
    extern bool started, safe;
    extern std::string startup_error;
    extern DWORD main_thread;
    extern NcClientServices services;
    extern Loaded* drawing;
    extern uint64_t event_serial;
    extern bool dispatching_events;
    extern std::vector<DrawOperation> draw_operations;
    extern size_t draw_text_bytes;

    bool available(const Loaded* plugin);
    bool permitted(const Loaded* plugin, uint32_t permission);
    std::string text(const char* value, size_t limit = 4096);
    std::string cvar_name(const char* value);
    void discover(bool verify_files = false);
    Json item_json(const Item& item);
    std::vector<Item> selection(const char* raw);
    int32_t setting_value(const Loaded& plugin, const Json& control);
    void validate_settings(const Json&);
    void validate_profile(const Json&);
    void migrate_settings();
    void load_settings(Loaded&);
    void save_settings(Loaded&, Json);
    fs::path settings_path(const Loaded&);
    NcHost make_host(Loaded& plugin);
    bool install_commands(Loaded& plugin);
    void fail(Loaded& plugin);
    void retire(Loaded& plugin);
    Json resource_stats(const Loaded* plugin = nullptr);
    extern double frame_callback_ms;
    extern uint64_t deferred_callbacks;
    extern std::array<bool, static_cast<size_t>(CallbackCategory::Count)> callback_categories;
    class CallbackScope
    {
        Loaded& plugin_;
        CallbackCategory category_;
        std::chrono::steady_clock::time_point begin_;
        std::string previous_;

    public:
        CallbackScope(Loaded&, CallbackCategory) noexcept;
        ~CallbackScope();
    };
    template <class F>
    int invoke(Loaded& p, CallbackCategory category, F&& callback)
    {
        CallbackScope scope(p, category);
        try
        {
            return callback();
        }
        catch (...)
        {
            return -1;
        }
    }
    bool callback_budget(CallbackCategory category);
    void reset_callback_budget();
    void recovery_start(bool interactive);
    bool recovery_pending();
    void recovery_acknowledge();
    void recovery_loading(const Loaded&);
    void recovery_running();
    void recovery_finish();
    bool enqueue(Loaded&, Event);
    void notify(Loaded&, const char*, const Json&, uint32_t permission = 0);
    void extensions_attach(Loaded&);
    void extensions_detach(Loaded&);
    void extensions_pump();
    void extensions_stop();
    const NcExtension* NC_CALL query_interface(void*, const char*, uint32_t);
    struct StoreCandidate
    {
        Json value;
        Budget memory;
    };
    StoreCandidate prepare_store(Loaded&, const Json& set, const Json& remove);
    void load_store(Loaded&);
    fs::path store_path(const Loaded&);
    Json extension_messages(Loaded&, const std::string&, const Json&);
    Json extension_ui(Loaded&, const std::string&, const Json&);
    Json extension_services(Loaded&, const std::string&, const Json&);
    bool Services_IsRequestPending(const Loaded& provider, uint64_t request_id);
    std::string service_completion(Loaded&, const std::string&, const Json&);
} // namespace plugins::runtime
