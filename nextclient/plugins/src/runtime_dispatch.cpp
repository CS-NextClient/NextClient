#include "runtime_internal.h"
#include <nextclient/events.h>
#include <cmath>
#include <chrono>

namespace plugins::runtime
{
    bool enqueue(Loaded& p, Event event)
    {
        if (!permitted(&p, event.permission) || !p.api.event)
            return false;
        if (p.pending.size() >= 2048 || p.pending_bytes + event.json.size() > 1024 * 1024)
        {
            ++p.dropped;
            if (!p.first_dropped)
                p.first_dropped = event.serial;
            p.last_dropped = event.serial;
            return false;
        }
        p.pending_bytes += event.json.size();
        p.pending.push_back(std::move(event));
        return true;
    }
    void notify(Loaded& p, const char* name, const Json& value, uint32_t permission)
    {
        enqueue(p, {++event_serial, name, tao::json::to_string(value), {}, permission, true});
    }
    void fail(Loaded& p)
    {
        p.host.log(p.host.context, "Callback failed; callbacks disabled until restart.");
        p.failed = true;
        // Successful dependencies always precede their dependents, so one forward
        // pass retires the entire dependent chain before another callback can run.
        for (auto& dependent : loaded)
            for (const auto& relation : dependent->item.manifest.required)
                for (const auto& provider : loaded)
                    if (provider->failed && provider->item.manifest.id == relation.id)
                        dependent->failed = true;
        for (auto& plugin : loaded)
            if (plugin->failed)
                extensions_detach(*plugin);
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

void nc_runtime_command(NcCommand* command, const NcPlayer* player)
{
    for (auto& p : loaded)
        if (!p->failed && p->api.command)
        {
            auto before = *command;
            int result = p->api.command(command, player);
            bool finite = std::isfinite(command->forward_move) && std::isfinite(command->side_move) && std::isfinite(command->up_move);
            for (auto f : command->view_angles)
                finite &= std::isfinite(f);
            if (result != 0 || !finite || command->size != sizeof(NcCommand) || command->buttons > 65535)
            {
                *command = before;
                fail(*p);
            }
            if (!permitted(p.get(), NC_PERMISSION_PLAYER_WRITE))
                *command = before;
        }
}

void nc_runtime_bind_client(const NcClientServices* value)
{
    services = value ? *value : NcClientServices{};
    for (auto& p : loaded)
    {
        if (!p->failed && !install_commands(*p))
            fail(*p);
        if (!p->failed && services.watch_message)
        {
            for (const auto& name : p->messages)
                services.watch_message(name.c_str());
            for (const auto& [name, filter] : p->filters)
                services.watch_message(name.c_str());
        }
    }
}
void nc_runtime_console(int32_t argc, const char* const* argv)
{
    if (GetCurrentThreadId() != main_thread || argc < 1 || argc > 64 || !argv || !argv[0])
        return;
    for (auto& p : loaded)
        if (!p->failed && p->api.console_command)
            for (const auto& id : p->commands)
                if (!_stricmp(("nc." + p->item.manifest.id + "." + id).c_str(), argv[0]))
                {
                    if (p->api.console_command(id.c_str(), argc - 1, argv + 1) != 0)
                        fail(*p);
                    return;
                }
}
void nc_runtime_draw(const NcDrawContext* context)
{
    if (GetCurrentThreadId() != main_thread || !context || context->size != sizeof(*context))
        return;
    for (auto& p : loaded)
        if (permitted(p.get(), NC_PERMISSION_UI_DRAW) && p->api.draw)
        {
            draw_operations.clear();
            draw_text_bytes = 0;
            drawing = p.get();
            const int result = p->api.draw(context);
            drawing = nullptr;
            if (result != 0)
                fail(*p);
            else
                for (const auto& op : draw_operations)
                {
                    if (op.is_text && services.draw_text)
                        services.draw_text(op.x, op.y, op.text.c_str(), op.rgba);
                    else if (!op.is_text && services.draw_rect)
                        services.draw_rect(op.x, op.y, op.width, op.height, op.rgba);
                }
        }
    draw_operations.clear();
}
void nc_runtime_frame(const NcSession* context)
{
    if (GetCurrentThreadId() != main_thread || !context || context->size != sizeof(*context))
        return;
    if (dispatching_events)
        return;
    dispatching_events = true;
    extensions_pump();
    const auto last_serial = event_serial;
    for (auto& p : loaded)
    {
        const auto start = std::chrono::steady_clock::now();
        if (available(p.get()) && p->api.event && p->first_dropped)
        {
            auto loss = tao::json::to_string(Json{{"dropped", p->dropped}, {"first", p->first_dropped}, {"last", p->last_dropped}});
            p->first_dropped = p->last_dropped = 0;
            if (p->api.event("sdk.overflow", loss.c_str()) != 0)
                fail(*p);
        }
        for (size_t count = 0; !p->pending.empty() && !p->failed && count < 128; ++count)
        {
            if (p->pending.front().serial > last_serial)
                break;
            auto event = std::move(p->pending.front());
            p->pending.pop_front();
            p->pending_bytes -= event.json.size();
            auto& subscriptions = event.cvar.empty() ? p->events : p->cvar_watches;
            auto found = subscriptions.find(event.cvar.empty() ? event.name : event.cvar);
            if ((event.direct || (found != subscriptions.end() && event.serial >= found->second)) && permitted(p.get(), event.permission) &&
                p->api.event)
            {
                const auto begin = std::chrono::steady_clock::now();
                const auto result = p->api.event(event.name.c_str(), event.json.c_str());
                p->callback_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
                ++p->delivered;
                if (p->callback_ms > 2)
                    ++p->slow_callbacks;
                if (result != 0)
                    fail(*p);
            }
            if (std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(2))
                break;
        }
    }
    for (auto& p : loaded)
        if (!p->failed && p->api.frame && p->api.frame(context) != 0)
            fail(*p);
    dispatching_events = false;
}
void nc_runtime_pump()
{
    if (GetCurrentThreadId() != main_thread || !started)
        return;
    NcSession session{};
    session.size = sizeof(session);
    if (services.get_session)
        services.get_session(&session);
    nc_runtime_frame(&session);
}
int32_t nc_runtime_ui_hidden(uint32_t element)
{
    for (const auto& p : loaded)
        if (permitted(p.get(), NC_PERMISSION_UI_HIDE) && (p->hidden_ui & element))
            return 1;
    return 0;
}

void nc_runtime_event(const char* raw, const char* payload)
{
    if (GetCurrentThreadId() != main_thread || !started)
        return;
    try
    {
        const auto name = text(raw, 64);
        if (event_permission(name) == UINT32_MAX || name == "cvar.changed")
            return;
        auto json = text(payload, 65536);
        parse(json);
        const auto serial = ++event_serial;
        for (auto& p : loaded)
            if (auto it = p->events.find(name); it != p->events.end() && serial >= it->second)
                enqueue(*p, {serial, name, json, {}, event_permission(name)});
    }
    catch (...)
    {}
}
void nc_runtime_cvar_changed(const char* raw, const char* before, const char* after)
{
    if (GetCurrentThreadId() != main_thread || !started)
        return;
    try
    {
        const auto name = cvar_name(raw), old_value = text(before, 65535), new_value = text(after, 65535);
        if (name.empty() || old_value == new_value)
            return;
        auto json = tao::json::to_string(Json{{"name", name}, {"old", old_value}, {"value", new_value}});
        const auto serial = ++event_serial;
        for (auto& p : loaded)
            if (auto it = p->cvar_watches.find(name); it != p->cvar_watches.end() && serial >= it->second)
                enqueue(*p, {serial, "cvar.changed", json, name, NC_PERMISSION_CVARS_READ});
    }
    catch (...)
    {}
}
