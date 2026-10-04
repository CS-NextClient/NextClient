#pragma once
#include "plugin.h"
#include <exception>
#include <string>

namespace nextclient
{
    // A plugin implements only the callbacks it needs. Exceptions never escape to
    // the host; a failed callback retires the plugin for the rest of this process.
    class Plugin
    {
    protected:
        const NcHost* host_{};

    public:
        const NcExtension* query_interface(const char* name, uint32_t version = 1) const
        {
            return host_->query_interface(host_->context, name, version);
        }
        std::string extension(const char* name, const char* operation, const char* json = "{}") const
        {
            const auto* api = query_interface(name);
            if (!api)
                throw std::exception();
            const auto handle = api->call(host_->context, operation, json);
            if (!handle)
                throw std::exception();
            struct ResultGuard
            {
                const NcExtension* api;
                void* context;
                uint64_t handle;
                ~ResultGuard()
                {
                    api->release_result(context, handle);
                }
            } guard{api, host_->context, handle};
            const auto size = api->read_result(host_->context, handle, nullptr, 0);
            if (!size || size > 1024 * 1024)
                throw std::exception();
            std::string result(size, '\0');
            if (api->read_result(host_->context, handle, result.data(), size) != size)
                throw std::exception();
            result.pop_back();
            return result;
        }
        virtual ~Plugin() = default;
        virtual void load() {}
        virtual void unload() {}
        virtual void command(NcCommand&, const NcPlayer&) {}
        virtual void setting_changed(const char*, int32_t) {}
        virtual void action(const char*) {}
        virtual void console_command(const char*, int32_t, const char* const*) {}
        virtual void draw(const NcDrawContext&) {}
        virtual void frame(const NcSession&) {}
        virtual void event(const char*, const char*) {}
        void attach(const NcHost* host)
        {
            host_ = host;
        }
        void log(const char* text) const
        {
            host_->log(host_->context, text);
        }
        void tab(const char* id, const char* en, const char* ru = "") const
        {
            if (!host_->add_tab(host_->context, id, en, ru))
                throw std::exception();
        }
        void control(NcControl value) const
        {
            value.size = sizeof(value);
            if (!host_->add_control(host_->context, &value))
                throw std::exception();
        }
        void checkbox(const char* id, const char* tab_id, const char* en, const char* ru = "", bool initial = false) const
        {
            control({sizeof(NcControl), id, tab_id, NC_CHECKBOX, en, ru, initial ? 1 : 0, 0, 1, "", ""});
        }
        int32_t setting(const char* id, int32_t fallback = 0) const
        {
            return host_->get_setting(host_->context, id, fallback);
        }
        uint32_t permissions() const
        {
            return host_->permissions(host_->context);
        }
        bool register_setting(const char* id, int32_t initial, int32_t minimum, int32_t maximum) const
        {
            return host_->register_setting(host_->context, id, initial, minimum, maximum) != 0;
        }
        bool set_setting(const char* id, int32_t value) const
        {
            return host_->set_setting(host_->context, id, value) != 0;
        }
        bool register_command(const char* id) const
        {
            return host_->register_command(host_->context, id) != 0;
        }
        bool player(NcPlayerState& value) const
        {
            value = {};
            value.size = sizeof(value);
            return host_->get_player(host_->context, &value) != 0;
        }
        bool entity(int32_t index, NcEntity& value) const
        {
            value = {};
            value.size = sizeof(value);
            return host_->get_entity(host_->context, index, &value) != 0;
        }
        bool weapon(int32_t id, NcWeapon& value) const
        {
            value = {};
            value.size = sizeof(value);
            return host_->get_weapon(host_->context, id, &value) != 0;
        }
        bool read_cvar(const char* name, std::string& value) const
        {
            value.clear();
            auto size = host_->read_cvar(host_->context, name, nullptr, 0);
            if (!size || size > 65536)
                return false;
            value.resize(size);
            if (host_->read_cvar(host_->context, name, value.data(), size) != size)
            {
                value.clear();
                return false;
            }
            value.resize(size - 1);
            return true;
        }
        bool write_cvar(const char* name, const char* value) const
        {
            return host_->write_cvar(host_->context, name, value) != 0;
        }
        bool hide_ui(uint32_t element, bool hide) const
        {
            return host_->hide_ui(host_->context, element, hide) != 0;
        }
        bool draw_rect(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t rgba) const
        {
            return host_->draw_rect(host_->context, x, y, width, height, rgba) != 0;
        }
        bool session(NcSession& value) const
        {
            value = {};
            value.size = sizeof(value);
            return host_->get_session(host_->context, &value) != 0;
        }
        bool player_info(int32_t index, NcPlayerInfo& value) const
        {
            value = {};
            value.size = sizeof(value);
            return host_->get_player_info(host_->context, index, &value) != 0;
        }
        bool world_to_screen(const float (&world)[3], float (&screen)[2]) const
        {
            return host_->world_to_screen(host_->context, world, screen) != 0;
        }
        bool measure_text(const char* text, int32_t& width, int32_t& height) const
        {
            return host_->measure_text(host_->context, text, &width, &height) != 0;
        }
        bool draw_text(int32_t x, int32_t y, const char* text, uint32_t rgb) const
        {
            return host_->draw_text(host_->context, x, y, text, rgb) != 0;
        }
        bool play_sound(const char* path, float volume = 1.0f) const
        {
            return host_->play_sound(host_->context, path, volume) != 0;
        }
        bool subscribe_event(const char* name, bool enable = true) const
        {
            return host_->subscribe_event(host_->context, name, enable) != 0;
        }
        template <class Read>
        static bool json_result(Read read, std::string& value)
        {
            value.clear();
            const auto size = read(nullptr, 0);
            if (!size || size > 1024 * 1024 + 1)
                return false;
            value.resize(size);
            if (read(value.data(), size) != size)
            {
                value.clear();
                return false;
            }
            value.resize(size - 1);
            return true;
        }
        bool game_data(const char* section, int32_t index, std::string& json) const
        {
            return json_result([&](char* out, uint32_t size) { return host_->game_data(host_->context, section, index, out, size); }, json);
        }
        bool create_cvar(const char* id, const char* initial, bool archive = false) const
        {
            return host_->create_cvar(host_->context, id, initial, archive) != 0;
        }
        bool watch_cvar(const char* name, bool enable = true) const
        {
            return host_->watch_cvar(host_->context, name, enable) != 0;
        }
        bool send_chat(const char* text, bool team = false) const
        {
            return host_->send_chat(host_->context, text, team) != 0;
        }
        bool connect(const char* host, uint32_t port = 27015) const
        {
            return host_->connect(host_->context, host, port) != 0;
        }
        bool disconnect() const
        {
            return host_->disconnect(host_->context) != 0;
        }
        bool store_get(const char* key, std::string& json) const
        {
            return json_result([&](char* out, uint32_t size) { return host_->store_get(host_->context, key, out, size); }, json);
        }
        bool store_set(const char* key, const char* json) const
        {
            return host_->store_set(host_->context, key, json) != 0;
        }
        bool store_delete(const char* key) const
        {
            return host_->store_delete(host_->context, key) != 0;
        }
        bool store_keys(std::string& json) const
        {
            return json_result([&](char* out, uint32_t size) { return host_->store_keys(host_->context, out, size); }, json);
        }
        bool console_print(const char* text) const
        {
            return host_->console_print(host_->context, text) != 0;
        }
        bool chat_print(const char* text) const
        {
            return host_->chat_print(host_->context, text) != 0;
        }
    };
    template <class T>
    struct Adapter
    {
        static inline T instance;
        template <class F>
        static int32_t protect(F fn) noexcept
        {
            try
            {
                fn();
                return 0;
            }
            catch (...)
            {
                return -1;
            }
        }
        static const NcPlugin* entry() noexcept
        {
            static const NcPlugin api{
                sizeof(NcPlugin),
                NC_ABI_VERSION,
                NC_API_VERSION,
                [](const NcHost* h) -> int32_t {
                    if (!h || h->size < sizeof(NcHost) || h->abi != NC_ABI_VERSION || h->api < NC_API_VERSION)
                        return -1;
                    return protect([&] {
                        instance.attach(h);
                        instance.load();
                    });
                },
                [] { protect([] { instance.unload(); }); },
                [](NcCommand* c, const NcPlayer* p) -> int32_t { return protect([&] { instance.command(*c, *p); }); },
                [](const char* id, int32_t v) -> int32_t { return protect([&] { instance.setting_changed(id, v); }); },
                [](const char* id) -> int32_t { return protect([&] { instance.action(id); }); },
                [](const char* id, int32_t argc, const char* const* argv) -> int32_t {
                    return protect([&] { instance.console_command(id, argc, argv); });
                },
                [](const NcDrawContext* context) -> int32_t { return protect([&] { instance.draw(*context); }); },
                [](const NcSession* context) -> int32_t { return protect([&] { instance.frame(*context); }); },
                [](const char* name, const char* json) -> int32_t { return protect([&] { instance.event(name, json); }); }
            };
            return &api;
        }
    };
} // namespace nextclient
#define NC_PLUGIN(type)                                                     \
    extern "C" NC_EXPORT const NcPlugin* NC_CALL nc_plugin_entry() noexcept \
    {                                                                       \
        return nextclient::Adapter<type>::entry();                          \
    }
