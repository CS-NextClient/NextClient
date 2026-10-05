#include "runtime_internal.h"
#include "plugin_limits.h"
#include <nextclient/events.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace plugins::runtime
{
    const std::set<std::string> builtin_tabs{"multiplayer", "game", "keyboard", "mouse", "audio", "video", "voice", "miscellaneous"};
    void NC_CALL log_message(void* ctx, const char* raw)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p))
                return;
            auto s = "[plugin " + p->item.manifest.id + "] " + text(raw) + "\n";
            OutputDebugStringA(s.c_str());
        }
        catch (...)
        {}
    }
    int32_t NC_CALL add_tab(void* ctx, const char* id, const char* en, const char* ru)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            auto name = text(id, 96);
            if (!permitted(p, NC_PERMISSION_UI_SETTINGS) || !p->registering || !valid_id(name) || builtin_tabs.count(name) ||
                p->tabs.get_array().size() >= 16)
                return 0;
            for (const auto& t : p->tabs.get_array())
                if (t.at("id") == name)
                    return 0;
            auto tab = Json{{"id", name}, {"en", text(en, 128)}, {"ru", text(ru, 128)}};
            p->registration_memory.resize(p->registration_memory.size() + json_memory(tab));
            p->tabs.push_back(std::move(tab));
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL register_setting(void* ctx, const char* raw, int32_t initial, int32_t minimum, int32_t maximum)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto id = text(raw, 96);
            if (!available(p) || !p->registering || !valid_id(id) || minimum > initial || initial > maximum)
                return 0;
            for (const auto& value : p->settings.get_array())
                if (value.at("id") == id)
                    return value.at("initial") == initial && value.at("min") == minimum && value.at("max") == maximum;
            if (p->settings.get_array().size() >= 128)
                return 0;
            auto spec = Json{{"id", id}, {"initial", initial}, {"min", minimum}, {"max", maximum}};
            Budget memory(json_memory(spec));
            p->settings.push_back(std::move(spec));
            p->registration_memory.absorb(std::move(memory));
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL add_control(void* ctx, const NcControl* c)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!permitted(p, NC_PERMISSION_UI_SETTINGS) || !p->registering || !c || c->size < sizeof(NcControl) ||
                p->controls.get_array().size() >= 128)
                return 0;
            auto id = text(c->id, 96), tab = text(c->tab, 96);
            if (!valid_id(id) || !valid_id(tab) || c->kind < NC_CHECKBOX || c->kind > NC_BUTTON || c->minimum > c->maximum ||
                c->initial < c->minimum || c->initial > c->maximum)
                return 0;
            bool exists = builtin_tabs.count(tab) != 0;
            for (const auto& t : p->tabs.get_array())
                if (t.at("id") == tab)
                    exists = true;
            if (!exists)
                return 0;
            for (const auto& value : p->controls.get_array())
                if (value.at("id") == id)
                    return 0;
            auto en = text(c->choices_en), ru = text(c->choices_ru);
            if (c->kind == NC_CHECKBOX && (c->minimum != 0 || c->maximum != 1))
                return 0;
            if (c->kind == NC_SLIDER && (static_cast<int64_t>(c->maximum) - c->minimum > 1000000))
                return 0;
            if (c->kind == NC_CHOICE &&
                (en.empty() || c->minimum != 0 || c->maximum != static_cast<int32_t>(std::count(en.begin(), en.end(), '\n')) ||
                 (!ru.empty() && std::count(ru.begin(), ru.end(), '\n') != c->maximum)))
                return 0;
            auto control = Json{
                {"id", id},
                {"tab", tab},
                {"kind", c->kind},
                {"en", text(c->label_en, 256)},
                {"ru", text(c->label_ru, 256)},
                {"initial", c->initial},
                {"min", c->minimum},
                {"max", c->maximum},
                {"choices_en", en},
                {"choices_ru", ru}
            };
            Budget memory(8192 + 2 * (en.capacity() + ru.capacity()));
            // Validate and allocate before registering the paired setting. Moving
            // the prepared JSON into this reserved slot cannot allocate afterwards.
            p->controls.get_array().reserve(p->controls.get_array().size() + 1);
            if (c->kind != NC_BUTTON && !register_setting(ctx, c->id, c->initial, c->minimum, c->maximum))
                return 0;
            p->controls.push_back(std::move(control));
            p->registration_memory.absorb(std::move(memory));
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t setting_value(const Loaded& p, const Json& c)
    {
        int32_t value = c.at("initial").as<int32_t>();
        if (auto v = p.values.find(c.at("id").get_string()))
            value = static_cast<int32_t>(integer(*v, INT32_MIN, INT32_MAX));
        return std::clamp(value, c.at("min").as<int32_t>(), c.at("max").as<int32_t>());
    }
    int32_t NC_CALL get_setting(void* ctx, const char* raw, int32_t fallback)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p))
                return fallback;
            auto id = text(raw, 96);
            for (const auto& c : p->settings.get_array())
                if (c.at("id") == id)
                    return setting_value(*p, c);
        }
        catch (...)
        {}
        return fallback;
    }
    uint32_t NC_CALL permissions(void* ctx)
    {
        auto* p = static_cast<Loaded*>(ctx);
        return available(p) ? p->item.manifest.permissions : 0;
    }
    int32_t NC_CALL set_setting(void* ctx, const char* raw, int32_t value)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p))
                return 0;
            const auto id = text(raw, 96);
            for (const auto& spec : p->settings.get_array())
                if (spec.at("id") == id && value >= spec.at("min").as<int32_t>() && value <= spec.at("max").as<int32_t>())
                {
                    auto next = p->values;
                    next[id] = value;
                    save_settings(*p, std::move(next));
                    return 1;
                }
        }
        catch (...)
        {}
        return 0;
    }
    int32_t NC_CALL register_command(void* ctx, const char* raw)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto id = text(raw, 48);
            if (!available(p) || !p->registering || !valid_id(id) || id.find('.') != std::string::npos || p->commands.size() >= 32 ||
                std::find(p->commands.begin(), p->commands.end(), id) != p->commands.end())
                return 0;
            p->commands.push_back(id);
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL get_player(void* ctx, NcPlayerState* value)
    {
        if (!available(static_cast<Loaded*>(ctx)) || !value || value->size != sizeof(*value) || !services.get_player)
            return 0;
        return services.get_player(value);
    }
    int32_t NC_CALL get_entity(void* ctx, int32_t index, NcEntity* value)
    {
        if (!available(static_cast<Loaded*>(ctx)) || !value || value->size != sizeof(*value) || !services.get_entity)
            return 0;
        return services.get_entity(index, value);
    }
    int32_t NC_CALL get_weapon(void* ctx, int32_t id, NcWeapon* value)
    {
        if (!available(static_cast<Loaded*>(ctx)) || !value || value->size != sizeof(*value) || !services.get_weapon)
            return 0;
        return services.get_weapon(id, value);
    }
    uint32_t NC_CALL read_cvar(void* ctx, const char* raw, char* buffer, uint32_t capacity)
    {
        if (buffer && capacity)
            buffer[0] = 0;
        try
        {
            if (!permitted(static_cast<Loaded*>(ctx), NC_PERMISSION_CVARS_READ) || !services.read_cvar)
                return 0;
            const auto name = text(raw, max_cvar_name_length);
            if (name.empty() || capacity > 65536 || (!buffer && capacity))
                return 0;
            return services.read_cvar(name.c_str(), buffer, capacity);
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL write_cvar(void* ctx, const char* raw, const char* raw_value)
    {
        try
        {
            if (!permitted(static_cast<Loaded*>(ctx), NC_PERMISSION_CVARS_WRITE) || !services.write_cvar)
                return 0;
            const auto name = text(raw, max_cvar_name_length), value = text(raw_value);
            return !name.empty() && services.write_cvar(name.c_str(), value.c_str());
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL hide_ui(void* ctx, uint32_t element, int32_t hide)
    {
        auto* p = static_cast<Loaded*>(ctx);
        if (!permitted(p, NC_PERMISSION_UI_HIDE) || (element != NC_UI_HUD && element != NC_UI_CROSSHAIR && element != NC_UI_HEALTH &&
                                                     element != NC_UI_RADAR && element != NC_UI_DEATH_NOTICES))
            return 0;
        if (hide)
            p->hidden_ui |= element;
        else
            p->hidden_ui &= ~element;
        return 1;
    }
    int32_t NC_CALL draw_rect(void* ctx, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t rgba)
    {
        auto* p = static_cast<Loaded*>(ctx);
        if (!permitted(p, NC_PERMISSION_UI_DRAW) || drawing != p || !services.draw_rect || draw_operations.size() >= 4096 || width < 0 ||
            height < 0 || width > 32768 || height > 32768 || x < -32768 || x > 32768 || y < -32768 || y > 32768)
            return 0;
        draw_operations.push_back({x, y, width, height, rgba});
        return 1;
    }
    int32_t NC_CALL get_session(void* ctx, NcSession* value)
    {
        if (!available(static_cast<Loaded*>(ctx)) || !value || value->size != sizeof(*value) || !services.get_session)
            return 0;
        return services.get_session(value);
    }
    int32_t NC_CALL get_player_info(void* ctx, int32_t index, NcPlayerInfo* value)
    {
        if (!available(static_cast<Loaded*>(ctx)) || !value || value->size != sizeof(*value) || !services.get_player_info)
            return 0;
        return services.get_player_info(index, value);
    }
    int32_t NC_CALL world_to_screen(void* ctx, const float* world, float* screen)
    {
        if (screen)
            screen[0] = screen[1] = 0;
        if (!available(static_cast<Loaded*>(ctx)) || !world || !screen || !services.world_to_screen || !std::isfinite(world[0]) ||
            !std::isfinite(world[1]) || !std::isfinite(world[2]))
            return 0;
        float result[2]{};
        if (!services.world_to_screen(world, result) || !std::isfinite(result[0]) || !std::isfinite(result[1]))
            return 0;
        std::copy_n(result, 2, screen);
        return 1;
    }
    bool single_line(const std::string& value)
    {
        return value.find_first_of("\r\n\t") == std::string::npos;
    }
    int32_t NC_CALL measure_text(void* ctx, const char* raw, int32_t* width, int32_t* height)
    {
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        try
        {
            if (!available(static_cast<Loaded*>(ctx)) || !raw || !width || !height || !services.measure_text)
                return 0;
            const auto value = text(raw, 1024);
            return single_line(value) && services.measure_text(value.c_str(), width, height);
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL draw_text(void* ctx, int32_t x, int32_t y, const char* raw, uint32_t rgb)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!permitted(p, NC_PERMISSION_UI_DRAW) || drawing != p || !services.draw_text || !raw || draw_operations.size() >= 4096 ||
                x < -32768 || x > 32768 || y < -32768 || y > 32768 || rgb > 0xffffff)
                return 0;
            auto value = text(raw, 1024);
            if (!single_line(value) || draw_text_bytes + value.size() > 65536)
                return 0;
            const auto bytes = value.size();
            draw_operations.push_back({x, y, 0, 0, rgb, true, std::move(value)});
            draw_text_bytes += bytes;
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL play_sound(void* ctx, const char* raw, float volume)
    {
        try
        {
            if (!permitted(static_cast<Loaded*>(ctx), NC_PERMISSION_AUDIO_PLAY) || !services.play_sound || !std::isfinite(volume) ||
                volume < 0 || volume > 1)
                return 0;
            const auto path = text(raw, 240);
            if (path.size() < 5 || path.front() == '/' ||
                path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_/.-") != std::string::npos ||
                _stricmp(path.c_str() + path.size() - 4, ".wav"))
                return 0;
            size_t start = 0;
            while (start < path.size())
            {
                const auto end = path.find('/', start);
                const auto part = path.substr(start, end - start);
                if (part.empty() || part == "." || part == "..")
                    return 0;
                if (end == std::string::npos)
                    break;
                start = end + 1;
            }
            services.play_sound(path.c_str(), volume);
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL console_print(void* ctx, const char* raw)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p) || !services.console_print || !raw)
                return 0;
            const auto value = text(raw) + "\n";
            services.console_print(value.c_str());
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL chat_print(void* ctx, const char* raw)
    {
        try
        {
            if (!permitted(static_cast<Loaded*>(ctx), NC_PERMISSION_CHAT_PRINT) || !services.chat_print || !raw)
                return 0;
            const auto value = text(raw, 190);
            if (value.empty())
                return 0;
            for (unsigned char c : value)
                if ((c < 32 && c != 1 && c != 3 && c != 4) || c == 127)
                    return 0;
            return services.chat_print(value.c_str());
        }
        catch (...)
        {
            return 0;
        }
    }
    uint32_t copy_result(const std::string& value, char* buffer, uint32_t capacity)
    {
        const auto required = static_cast<uint32_t>(value.size() + 1);
        if (buffer && capacity >= required)
            std::memcpy(buffer, value.c_str(), required);
        return required;
    }
    bool read_buffer(char* buffer, uint32_t capacity)
    {
        if (buffer && capacity)
            buffer[0] = 0;
        return capacity <= 1024 * 1024 + 1 && (buffer || !capacity);
    }
    std::string cvar_name(const char* raw)
    {
        auto name = text(raw, max_cvar_name_length);
        for (char& c : name)
            if (c >= 'A' && c <= 'Z')
                c += 'a' - 'A';
        if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") != std::string::npos)
            return {};
        return name;
    }
    int32_t NC_CALL subscribe_event(void* ctx, const char* raw, int32_t enable)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto name = text(raw, 64);
            const auto permission = event_permission(name);
            if (!available(p) || permission == UINT32_MAX || name == "cvar.changed" || !permitted(p, permission))
                return 0;
            if (enable)
                p->events.try_emplace(name, event_serial + 1);
            else
                p->events.erase(name);
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    uint32_t NC_CALL game_data(void* ctx, const char* raw, int32_t index, char* buffer, uint32_t capacity)
    {
        if (!read_buffer(buffer, capacity))
            return 0;
        try
        {
            if (!available(static_cast<Loaded*>(ctx)) || !services.game_data)
                return 0;
            const auto section = text(raw, 32);
            constexpr std::string_view sections[]{
                "player", "entity", "entities", "weapon", "weapons", "ammo", "scoreboard", "match", "connection"
            };
            if (std::find(std::begin(sections), std::end(sections), section) == std::end(sections))
                return 0;
            return services.game_data(section.c_str(), index, buffer, capacity);
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL create_cvar(void* ctx, const char* raw, const char* initial, int32_t archive)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto id = text(raw, max_local_cvar_length), value = text(initial);
            if (!permitted(p, NC_PERMISSION_CVARS_CREATE) || !p->registering || !valid_id(id) || id.find('.') != std::string::npos ||
                !initial || (archive != 0 && archive != 1) || p->cvars.size() >= 32)
                return 0;
            const auto name = "nc." + p->item.manifest.id + "." + id;
            for (const auto& c : p->cvars)
                if (c.name == name)
                    return 0;
            p->registration_memory.resize(p->registration_memory.size() + string_memory(name) + string_memory(value) + 256);
            p->cvars.push_back({name, value, archive});
            return 1; // Installed after successful load, or when the client binds.
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL watch_cvar(void* ctx, const char* raw, int32_t enable)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto name = cvar_name(raw);
            if (!permitted(p, NC_PERMISSION_CVARS_READ) || name.empty())
                return 0;
            if (!enable)
            {
                p->cvar_watches.erase(name);
                return 1;
            }
            if (!p->cvar_watches.count(name) && p->cvar_watches.size() >= 64)
                return 0;
            p->cvar_watches.try_emplace(name, event_serial + 1);
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL send_chat(void* ctx, const char* raw, int32_t team)
    {
        try
        {
            if (!permitted(static_cast<Loaded*>(ctx), NC_PERMISSION_CHAT_SEND) || !services.send_chat || (team != 0 && team != 1))
                return 0;
            const auto value = text(raw, 190);
            if (value.empty() || value.find_first_of("\";\\") != std::string::npos)
                return 0;
            for (unsigned char c : value)
                if (c < 32 || c == 127)
                    return 0;
            return services.send_chat(value.c_str(), team);
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL connect(void* ctx, const char* raw, uint32_t port)
    {
        try
        {
            if (!permitted(static_cast<Loaded*>(ctx), NC_PERMISSION_CONNECTION_CONNECT) || !services.connect || !port || port > 65535)
                return 0;
            const auto host = text(raw, 253);
            if (host.empty() || host.front() == '-' || host.front() == '.' || host.back() == '-' || host.back() == '.' ||
                host.find("..") != std::string::npos ||
                host.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") != std::string::npos)
                return 0;
            return services.connect(host.c_str(), port);
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL disconnect(void* ctx)
    {
        return permitted(static_cast<Loaded*>(ctx), NC_PERMISSION_CONNECTION_DISCONNECT) && services.disconnect && services.disconnect();
    }
    fs::path store_path(const Loaded& p)
    {
        return root / L"plugins" / L"data" / ("plugin-" + p.item.manifest.id + ".json");
    }
    void load_store(Loaded& p)
    {
        if (p.store_loaded)
            return;
        auto value = parse(read_text(store_path(p)));
        if (!value.is_object() || value.get_object().size() > 1024)
            throw std::runtime_error("Invalid plugin store");
        replace_json(p.store, p.store_memory, std::move(value));
        p.store_loaded = true;
    }
    uint32_t NC_CALL store_get(void* ctx, const char* raw, char* buffer, uint32_t capacity)
    {
        if (!read_buffer(buffer, capacity))
            return 0;
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto key = text(raw, 96);
            if (!available(p) || !valid_id(key))
                return 0;
            load_store(*p);
            auto value = p->store.find(key);
            return value ? copy_result(tao::json::to_string(*value), buffer, capacity) : 0;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL store_set(void* ctx, const char* raw, const char* json)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto key = text(raw, 96);
            if (!available(p) || p->store_pending || !valid_id(key) || !json)
                return 0;
            auto value = parse(text(json, 65536));
            Budget argument(json_memory(value));
            load_store(*p);
            Budget working(json_memory(p->store));
            auto next = p->store;
            next[key] = std::move(value);
            if (next.get_object().size() > 1024 || tao::json::to_string(next).size() > 1024 * 1024)
                return 0;
            // Include the store's outer object in the depth limit, so a value
            // accepted now is still readable after restarting the client.
            parse(tao::json::to_string(next));
            Budget candidate(json_memory(next));
            write_json(store_path(*p), next);
            p->store = std::move(next);
            p->store_memory = std::move(candidate);
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    int32_t NC_CALL store_delete(void* ctx, const char* raw)
    {
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            const auto key = text(raw, 96);
            if (!available(p) || !valid_id(key))
                return 0;
            load_store(*p);
            if (!p->store.find(key))
                return 1;
            if (p->store_pending)
                return 0;
            Budget working(json_memory(p->store));
            auto next = p->store;
            next.erase(key);
            Budget candidate(json_memory(next));
            write_json(store_path(*p), next);
            p->store = std::move(next);
            p->store_memory = std::move(candidate);
            return 1;
        }
        catch (...)
        {
            return 0;
        }
    }
    uint32_t NC_CALL store_keys(void* ctx, char* buffer, uint32_t capacity)
    {
        if (!read_buffer(buffer, capacity))
            return 0;
        try
        {
            auto* p = static_cast<Loaded*>(ctx);
            if (!available(p))
                return 0;
            load_store(*p);
            Json keys = tao::json::empty_array;
            for (const auto& [key, value] : p->store.get_object())
                keys.push_back(key);
            return copy_result(tao::json::to_string(keys), buffer, capacity);
        }
        catch (...)
        {
            return 0;
        }
    }
    bool install_commands(Loaded& p)
    {
        if (services.create_cvar)
            for (const auto& c : p.cvars)
                if (!services.create_cvar(c.name.c_str(), c.initial.c_str(), c.archive))
                    return false;
        if (!services.register_command)
            return true; // Client may initialize after GameUI starts plugins.
        for (const auto& id : p.commands)
            if (!services.register_command(("nc." + p.item.manifest.id + "." + id).c_str()))
                return false;
        return true;
    }
    NcHost make_host(Loaded& plugin)
    {
        return {sizeof(NcHost),  NC_ABI_VERSION, NC_API_VERSION,   &plugin,        log_message,      add_tab,         add_control,
                get_setting,     permissions,    register_setting, set_setting,    register_command, get_player,      get_entity,
                get_weapon,      read_cvar,      write_cvar,       hide_ui,        draw_rect,        get_session,     get_player_info,
                world_to_screen, measure_text,   draw_text,        play_sound,     console_print,    subscribe_event, game_data,
                create_cvar,     watch_cvar,     send_chat,        connect,        disconnect,       store_get,       store_set,
                store_delete,    store_keys,     chat_print,       query_interface};
    }
} // namespace plugins::runtime
