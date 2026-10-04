#pragma once
#include "catalog.h"
#include "plugin_file.h"
#include "plugin_package.h"
#include <nextclient/runtime.h>
#include <deque>
#include <map>
#include <memory>
#include <set>

namespace plugins::runtime
{
    namespace fs = std::filesystem;
    struct Event
    {
        uint64_t serial{};
        std::string name, json, cvar;
        uint32_t permission{};
        bool direct{};
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
        size_t result_bytes{};
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
    };
    struct DrawOperation
    {
        int32_t x, y, width, height;
        uint32_t rgba;
        bool is_text{};
        std::string text;
    };

    extern fs::path root;
    extern Json profile, settings;
    extern std::vector<std::unique_ptr<Loaded>> loaded;
    extern std::vector<Item> observed;
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
    bool valid_id(const std::string& value);
    std::string cvar_name(const char* value);
    void discover();
    Json item_json(const Item& item);
    std::vector<Item> selection(const char* raw);
    int32_t setting_value(const Loaded& plugin, const Json& control);
    NcHost make_host(Loaded& plugin);
    bool install_commands(Loaded& plugin);
    void fail(Loaded& plugin);
    bool enqueue(Loaded&, Event);
    void notify(Loaded&, const char*, const Json&, uint32_t permission = 0);
    void extensions_attach(Loaded&);
    void extensions_detach(Loaded&);
    void extensions_pump();
    void extensions_stop();
    const NcExtension* NC_CALL query_interface(void*, const char*, uint32_t);
    void load_store(Loaded&);
    fs::path store_path(const Loaded&);
    Json extension_messages(Loaded&, const std::string&, const Json&);
    Json extension_ui(Loaded&, const std::string&, const Json&);
    Json extension_services(Loaded&, const std::string&, const Json&);
} // namespace plugins::runtime
